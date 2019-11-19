// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/adb_client_socket.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/address_list.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "third_party/blink/public/public_buildflags.h"

namespace {

const int kBufferSize = 16 * 1024;
const int kBufferGrowthRate = 16 * 1024;
const size_t kAdbDataChunkSize = 32 * 1024;
const char kOkayResponse[] = "OKAY";
const char kFailResponse[] = "FAIL";
const char kHostTransportCommand[] = "host:transport:%s";
const char kLocalAbstractCommand[] = "localabstract:%s";
const char kSyncCommand[] = "sync:";
const char kSendCommand[] = "SEND";
const char kDataCommand[] = "DATA";
const char kDoneCommand[] = "DONE";
const int kAdbFailure = 1;
const int kAdbSuccess = 0;

typedef base::Callback<void(int, const std::string&)> CommandCallback;
typedef base::Callback<void(int, net::StreamSocket*)> SocketCallback;
typedef base::Callback<void(const std::string&)> ParserCallback;

std::string EncodeMessage(const std::string& message) {
  static const char kHexChars[] = "0123456789ABCDEF";

  size_t length = message.length();
  std::string result(4, '\0');
  char b = reinterpret_cast<const char*>(&length)[1];
  result[0] = kHexChars[(b >> 4) & 0xf];
  result[1] = kHexChars[b & 0xf];
  b = reinterpret_cast<const char*>(&length)[0];
  result[2] = kHexChars[(b >> 4) & 0xf];
  result[3] = kHexChars[b & 0xf];
  return result + message;
}

class AdbTransportSocket : public AdbClientSocket {
 public:
  AdbTransportSocket(int port,
                     const std::string& serial,
                     const std::string& socket_name,
                     const SocketCallback& callback)
    : AdbClientSocket(port),
      serial_(serial),
      socket_name_(socket_name),
      callback_(callback) {
    Connect(base::BindOnce(&AdbTransportSocket::OnConnected,
                           base::Unretained(this)));
  }

 private:
  ~AdbTransportSocket() {}

  void OnConnected(int result) {
    if (!CheckNetResultOrDie(result))
      return;
    SendCommand(base::StringPrintf(kHostTransportCommand, serial_.c_str()),
                false, true,
                base::Bind(&AdbTransportSocket::SendLocalAbstract,
                           base::Unretained(this)));
  }

  void SendLocalAbstract(int result, const std::string& response) {
    if (!CheckNetResultOrDie(result))
      return;
    SendCommand(base::StringPrintf(kLocalAbstractCommand, socket_name_.c_str()),
                false, true,
                base::Bind(&AdbTransportSocket::OnSocketAvailable,
                           base::Unretained(this)));
  }

  void OnSocketAvailable(int result, const std::string& response) {
    if (!CheckNetResultOrDie(result))
      return;
    callback_.Run(net::OK, socket_.release());
    delete this;
  }

  bool CheckNetResultOrDie(int result) {
    if (result >= 0)
      return true;
    callback_.Run(result, NULL);
    delete this;
    return false;
  }

  std::string serial_;
  std::string socket_name_;
  SocketCallback callback_;
};

class HttpOverAdbSocket {
 public:
  HttpOverAdbSocket(int port,
                    const std::string& serial,
                    const std::string& socket_name,
                    const std::string& request,
                    const CommandCallback& callback)
    : request_(request),
      command_callback_(callback),
      body_pos_(0) {
    Connect(port, serial, socket_name);
  }

  HttpOverAdbSocket(int port,
                    const std::string& serial,
                    const std::string& socket_name,
                    const std::string& request,
                    const SocketCallback& callback)
    : request_(request),
      socket_callback_(callback),
      body_pos_(0) {
    Connect(port, serial, socket_name);
  }

 private:
  ~HttpOverAdbSocket() {
  }

  void Connect(int port,
               const std::string& serial,
               const std::string& socket_name) {
    AdbClientSocket::TransportQuery(
        port, serial, socket_name,
        base::Bind(&HttpOverAdbSocket::OnSocketAvailable,
                   base::Unretained(this)));
  }

  void OnSocketAvailable(int result,
                         net::StreamSocket* socket) {
    if (!CheckNetResultOrDie(result))
      return;

    socket_.reset(socket);

    scoped_refptr<net::StringIOBuffer> request_buffer =
        base::MakeRefCounted<net::StringIOBuffer>(request_);

    result = socket_->Write(request_buffer.get(), request_buffer->size(),
                            base::BindOnce(&HttpOverAdbSocket::ReadResponse,
                                           base::Unretained(this)),
                            TRAFFIC_ANNOTATION_FOR_TESTS);
    if (result != net::ERR_IO_PENDING)
      ReadResponse(result);
  }

  void ReadResponse(int result) {
    if (!CheckNetResultOrDie(result))
      return;

    scoped_refptr<net::IOBuffer> response_buffer =
        base::MakeRefCounted<net::IOBuffer>(kBufferSize);

    result = socket_->Read(
        response_buffer.get(), kBufferSize,
        base::BindOnce(&HttpOverAdbSocket::OnResponseData,
                       base::Unretained(this), response_buffer, -1));
    if (result != net::ERR_IO_PENDING)
      OnResponseData(response_buffer, -1, result);
  }

  void OnResponseData(scoped_refptr<net::IOBuffer> response_buffer,
                      int bytes_total,
                      int result) {
    if (!CheckNetResultOrDie(result))
      return;
    if (result == 0) {
      CheckNetResultOrDie(net::ERR_CONNECTION_CLOSED);
      return;
    }

    response_ += std::string(response_buffer->data(), result);
    int expected_length = 0;
    if (bytes_total < 0) {
      size_t content_pos = response_.find("Content-Length:");
      if (content_pos != std::string::npos) {
        size_t endline_pos = response_.find("\n", content_pos);
        if (endline_pos != std::string::npos) {
          std::string len = response_.substr(content_pos + 15,
                                             endline_pos - content_pos - 15);
          base::TrimWhitespaceASCII(len, base::TRIM_ALL, &len);
          if (!base::StringToInt(len, &expected_length)) {
            CheckNetResultOrDie(net::ERR_FAILED);
            return;
          }
        }
      }

      body_pos_ = response_.find("\r\n\r\n");
      if (body_pos_ != std::string::npos) {
        body_pos_ += 4;
        bytes_total = body_pos_ + expected_length;
      }
    }

    if (bytes_total == static_cast<int>(response_.length())) {
      if (!command_callback_.is_null())
        command_callback_.Run(body_pos_, response_);
      else
        socket_callback_.Run(net::OK, socket_.release());
      delete this;
      return;
    }

    result = socket_->Read(
        response_buffer.get(), kBufferSize,
        base::BindOnce(&HttpOverAdbSocket::OnResponseData,
                       base::Unretained(this), response_buffer, bytes_total));
    if (result != net::ERR_IO_PENDING)
      OnResponseData(response_buffer, bytes_total, result);
  }

  bool CheckNetResultOrDie(int result) {
    if (result >= 0)
      return true;
    if (!command_callback_.is_null())
      command_callback_.Run(result, std::string());
    else
      socket_callback_.Run(result, NULL);
    delete this;
    return false;
  }

  std::unique_ptr<net::StreamSocket> socket_;
  std::string request_;
  std::string response_;
  CommandCallback command_callback_;
  SocketCallback socket_callback_;
  size_t body_pos_;
};

class AdbQuerySocket : AdbClientSocket {
 public:
  AdbQuerySocket(int port,
                 const std::string& query,
                 const CommandCallback& callback)
      : AdbClientSocket(port),
        current_query_(0),
        callback_(callback) {
    queries_ = base::SplitString(
        query, "|", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (queries_.empty()) {
      CheckNetResultOrDie(net::ERR_INVALID_ARGUMENT);
      return;
    }
    Connect(
        base::BindOnce(&AdbQuerySocket::SendNextQuery, base::Unretained(this)));
  }

 private:
  ~AdbQuerySocket() {
  }

  void SendNextQuery(int result) {
    if (!CheckNetResultOrDie(result))
      return;
    std::string query = queries_[current_query_];
    if (query.length() > 0xFFFF) {
      CheckNetResultOrDie(net::ERR_MSG_TOO_BIG);
      return;
    }
    bool has_output = current_query_ == queries_.size() - 1;
    // The |shell| command is a special case because it is the only command that
    // doesn't include a length at the beginning of the data stream.
    bool has_length =
        !base::StartsWith(query, "shell:", base::CompareCase::SENSITIVE);
    SendCommand(
        query, has_output, has_length,
        base::Bind(&AdbQuerySocket::OnResponse, base::Unretained(this)));
  }

  void OnResponse(int result, const std::string& response) {
    if (++current_query_ < queries_.size()) {
      SendNextQuery(net::OK);
    } else {
      callback_.Run(result, response);
      delete this;
    }
  }

  bool CheckNetResultOrDie(int result) {
    if (result >= 0)
      return true;
    callback_.Run(result, std::string());
    delete this;
    return false;
  }

  std::vector<std::string> queries_;
  size_t current_query_;
  CommandCallback callback_;
};

// Implement the ADB protocol to send a file to the device.
// The protocol consists of the following steps:
// * Send "host:transport" command with device serial
// * Send "sync:" command to initialize file transfer
// * Send "SEND" command with name and mode of the file
// * Send "DATA" command one or more times for the file content
// * Send "DONE" command to indicate end of file transfer
// The first two commands use normal ADB command format implemented by
// AdbClientSocket::SendCommand. The remaining commands use a special
// format implemented by AdbSendFileSocket::SendPayload.
class AdbSendFileSocket : AdbClientSocket {
 public:
  AdbSendFileSocket(int port,
                    const std::string& serial,
                    const std::string& filename,
                    const std::string& content,
                    const CommandCallback& callback)
      : AdbClientSocket(port),
        serial_(serial),
        filename_(filename),
        content_(content),
        current_offset_(0),
        callback_(callback) {
    Connect(base::BindOnce(&AdbSendFileSocket::SendTransport,
                           base::Unretained(this)));
  }

 private:
  ~AdbSendFileSocket() {}

  void SendTransport(int result) {
    if (!CheckNetResultOrDie(result))
      return;
    SendCommand(
        base::StringPrintf(kHostTransportCommand, serial_.c_str()), false, true,
        base::Bind(&AdbSendFileSocket::SendSync, base::Unretained(this)));
  }

  void SendSync(int result, const std::string& response) {
    if (!CheckNetResultOrDie(result))
      return;
    SendCommand(
        kSyncCommand, false, true,
        base::Bind(&AdbSendFileSocket::SendSend, base::Unretained(this)));
  }

  void SendSend(int result, const std::string& response) {
    if (!CheckNetResultOrDie(result))
      return;
    // File mode. The following value is equivalent to S_IRUSR | S_IWUSR.
    // Can't use the symbolic names since they are not available on Windows.
    int mode = 0600;
    std::string payload = base::StringPrintf("%s,%d", filename_.c_str(), mode);
    SendPayload(kSendCommand, payload.length(), payload.c_str(),
                payload.length(),
                base::BindOnce(&AdbSendFileSocket::SendContent,
                               base::Unretained(this)));
  }

  void SendContent(int result) {
    if (!CheckNetResultOrDie(result))
      return;
    if (current_offset_ >= content_.length()) {
      SendDone();
      return;
    }
    size_t offset = current_offset_;
    size_t length = std::min(content_.length() - offset, kAdbDataChunkSize);
    current_offset_ += length;
    SendPayload(kDataCommand, length, content_.c_str() + offset, length,
                base::BindOnce(&AdbSendFileSocket::SendContent,
                               base::Unretained(this)));
  }

  void SendDone() {
    int data = time(NULL);
    SendPayload(kDoneCommand, data, nullptr, 0,
                base::BindOnce(&AdbSendFileSocket::ReadFinalResponse,
                               base::Unretained(this)));
  }

  void ReadFinalResponse(int result) {
    ReadResponse(callback_, false, false, result);
  }

  // Send a special payload command ("SEND", "DATA", or "DONE").
  // Each command consists of a command line, followed by a 4-byte integer
  // sent in raw little-endian format, followed by an optional payload.
  void SendPayload(const char* command,
                   int data,
                   const char* payload,
                   size_t payloadLength,
                   net::CompletionOnceCallback callback) {
    std::string buffer(command);
    for (int i = 0; i < 4; i++) {
      buffer.append(1, static_cast<char>(data & 0xff));
      data >>= 8;
    }
    if (payloadLength > 0)
      buffer.append(payload, payloadLength);

    scoped_refptr<net::StringIOBuffer> request_buffer =
        base::MakeRefCounted<net::StringIOBuffer>(buffer);

    net::CompletionRepeatingCallback copyable_callback =
        base::AdaptCallbackForRepeating(std::move(callback));
    int result =
        socket_->Write(request_buffer.get(), request_buffer->size(),
                       copyable_callback, TRAFFIC_ANNOTATION_FOR_TESTS);
    if (result != net::ERR_IO_PENDING)
      copyable_callback.Run(result);
  }

  bool CheckNetResultOrDie(int result) {
    if (result >= 0)
      return true;
    callback_.Run(result, NULL);
    delete this;
    return false;
  }

  std::string serial_;
  std::string filename_;
  std::string content_;
  size_t current_offset_;
  CommandCallback callback_;
};

}  // namespace

// static
void AdbClientSocket::AdbQuery(int port,
                               const std::string& query,
                               const CommandCallback& callback) {
  new AdbQuerySocket(port, query, callback);
}

// static
void AdbClientSocket::SendFile(int port,
                               const std::string& serial,
                               const std::string& filename,
                               const std::string& content,
                               const CommandCallback& callback) {
  new AdbSendFileSocket(port, serial, filename, content, callback);
}

#if BUILDFLAG(DEBUG_DEVTOOLS)
static void UseTransportQueryForDesktop(const SocketCallback& callback,
                                        net::StreamSocket* socket,
                                        int result) {
  callback.Run(result, socket);
}
#endif  // BUILDFLAG(DEBUG_DEVTOOLS)

// static
void AdbClientSocket::TransportQuery(int port,
                                     const std::string& serial,
                                     const std::string& socket_name,
                                     const SocketCallback& callback) {
#if BUILDFLAG(DEBUG_DEVTOOLS)
  if (serial.empty()) {
    // Use plain socket for remote debugging on Desktop (debugging purposes).
    int tcp_port = 0;
    if (!base::StringToInt(socket_name, &tcp_port))
      tcp_port = 9222;

    net::AddressList address_list = net::AddressList::CreateFromIPAddress(
        net::IPAddress::IPv4Localhost(), tcp_port);
    net::TCPClientSocket* socket = new net::TCPClientSocket(
        address_list, nullptr, nullptr, net::NetLogSource());
    socket->Connect(
        base::BindOnce(&UseTransportQueryForDesktop, callback, socket));
    return;
  }
#endif  // BUILDFLAG(DEBUG_DEVTOOLS)
  new AdbTransportSocket(port, serial, socket_name, callback);
}

// static
void AdbClientSocket::HttpQuery(int port,
                                const std::string& serial,
                                const std::string& socket_name,
                                const std::string& request_path,
                                const CommandCallback& callback) {
  new HttpOverAdbSocket(port, serial, socket_name, request_path,
      callback);
}

// static
void AdbClientSocket::HttpQuery(int port,
                                const std::string& serial,
                                const std::string& socket_name,
                                const std::string& request_path,
                                const SocketCallback& callback) {
  new HttpOverAdbSocket(port, serial, socket_name, request_path,
      callback);
}

AdbClientSocket::AdbClientSocket(int port) : port_(port) {}

AdbClientSocket::~AdbClientSocket() {
}

void AdbClientSocket::Connect(net::CompletionOnceCallback callback) {
  // In a IPv4/IPv6 dual stack environment, getaddrinfo for localhost could
  // only return IPv6 address while current adb (1.0.36) will always listen
  // on IPv4. So just try IPv4 first, then fall back to IPv6.
  net::IPAddressList list = {net::IPAddress::IPv4Localhost(),
                             net::IPAddress::IPv6Localhost()};
  net::AddressList ip_list = net::AddressList::CreateFromIPAddressList(
      list, "localhost");
  net::AddressList address_list = net::AddressList::CopyWithPort(
      ip_list, port_);

  socket_.reset(new net::TCPClientSocket(address_list, NULL, NULL,
                                         net::NetLogSource()));

  net::CompletionRepeatingCallback copyable_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  int result = socket_->Connect(copyable_callback);
  if (result != net::ERR_IO_PENDING)
    copyable_callback.Run(result);
}

void AdbClientSocket::SendCommand(const std::string& command,
                                  bool has_output,
                                  bool has_length,
                                  const CommandCallback& response_callback) {
  scoped_refptr<net::StringIOBuffer> request_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(EncodeMessage(command));
  int result = socket_->Write(
      request_buffer.get(), request_buffer->size(),
      base::BindOnce(&AdbClientSocket::ReadResponse, base::Unretained(this),
                     response_callback, has_output, has_length),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  if (result != net::ERR_IO_PENDING)
    ReadResponse(response_callback, has_output, has_length, result);
}

void AdbClientSocket::ReadResponse(const CommandCallback& response_callback,
                                   bool has_output,
                                   bool has_length,
                                   int result) {
  if (result < 0) {
    response_callback.Run(result, "IO error");
    return;
  }
  scoped_refptr<net::GrowableIOBuffer> socket_buffer =
      base::MakeRefCounted<net::GrowableIOBuffer>();
  socket_buffer->SetCapacity(kBufferSize);
  if (has_output) {
    const ParserCallback& parse_output_callback = base::Bind(
        &AdbClientSocket::ParseOutput, has_length, response_callback);
    int socket_result = socket_->Read(
        socket_buffer.get(), kBufferSize,
        base::BindOnce(&AdbClientSocket::ReadUntilEOF, base::Unretained(this),
                       parse_output_callback, response_callback,
                       socket_buffer));
    if (socket_result != net::ERR_IO_PENDING) {
      ReadUntilEOF(parse_output_callback, response_callback, socket_buffer,
                   socket_result);
    }
  } else {
    int socket_result =
        socket_->Read(socket_buffer.get(), kBufferSize,
                      base::BindOnce(&AdbClientSocket::ReadStatusOutput,
                                     response_callback, socket_buffer));
    if (socket_result != net::ERR_IO_PENDING) {
      ReadStatusOutput(response_callback, socket_buffer, socket_result);
    }
  }
}

void AdbClientSocket::ReadStatusOutput(
    const CommandCallback& response_callback,
    scoped_refptr<net::IOBuffer> socket_buffer,
    int socket_result) {
  // Sometimes adb-server responds with the 8 char string "OKAY\0\0\0\0" instead
  // of the expected "OKAY".
  if (socket_result >= 4 &&
      std::string(socket_buffer->data(), 4) == kOkayResponse) {
    response_callback.Run(kAdbSuccess, kOkayResponse);
    return;
  }
  response_callback.Run(kAdbFailure, kFailResponse);
}

void AdbClientSocket::ReadUntilEOF(
    const ParserCallback& parse_output_callback,
    const CommandCallback& response_callback,
    scoped_refptr<net::GrowableIOBuffer> socket_buffer,
    int socket_result) {
  if (socket_result < 0) {
    VLOG(3) << "IO error";
    response_callback.Run(socket_result, "IO error");
    return;
  } else if (socket_result > 0) {
    // We read in data. Let's try to read in more.
    socket_buffer->set_offset(socket_buffer->offset() + socket_result);
    if (socket_buffer->RemainingCapacity() == 0) {
      socket_buffer->SetCapacity(socket_buffer->capacity() + kBufferGrowthRate);
    }
    int new_socket_result = socket_->Read(
        socket_buffer.get(), socket_buffer->RemainingCapacity(),
        base::BindOnce(&AdbClientSocket::ReadUntilEOF, base::Unretained(this),
                       parse_output_callback, response_callback,
                       socket_buffer));
    if (new_socket_result != net::ERR_IO_PENDING) {
      ReadUntilEOF(parse_output_callback, response_callback, socket_buffer,
                   new_socket_result);
    }
  } else if (socket_result == 0) {
    // We hit EOF. The socket is closed on the other side.
    std::string adb_output(socket_buffer->StartOfBuffer(),
                           socket_buffer->offset());
    parse_output_callback.Run(adb_output);
  }
}

void AdbClientSocket::ParseOutput(bool has_length,
                                  const CommandCallback& response_callback,
                                  const std::string& adb_output) {
  int result = kAdbFailure;
  // Expected data format is
  // "OKAY<payload_length><payload>" if has_length
  // or "OKAY<payload>" if not has_length.
  // or "OKAY" if there is no output (regardless of has_length).
  // FAIL is returned instead of OKAY if there was an error.
  std::string output(adb_output);
  if (output.substr(0, 4) == kOkayResponse) {
    output = output.substr(4);
    result = kAdbSuccess;
  }
  if (output.substr(0, 4) == kFailResponse) {
    output = output.substr(4);
    result = kAdbFailure;
  }
  if (output.substr(0, 4) == kOkayResponse) {
    VLOG(3) << "ADB server responded with \"OKAYOKAY\" instead of \"OKAY\".";
    output = output.substr(4);
    // Note: It's unclear whether we should set result = kAdbSuccess here or
    // not. I've never seen "FAILOKAY" in the wild.
  }
  // Note that has_length=true implies that the payload length will be prepended
  // to the payload in the case that there is a payload. If there is no payload,
  // then there may not be a payload length.
  if (has_length && output.size() != 0) {
    if (output.size() >= 4) {
      // Just skip the hex string length. It is unnecessary since
      // EOF is sent after the message is complete.
      output = output.substr(4);
    } else {
      VLOG(3) << "Error: ADB server responded without the expected hexstring"
              << " length";
      result = kAdbFailure;
    }
  }
  response_callback.Run(result, output);
}
