// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/data_source.h"

#include <limits>
#include <optional>
#include <string_view>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/character_encoding.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/data_source_observer.h"
#include "components/exo/mime_utils.h"
#include "components/exo/security_delegate.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/icu/source/common/unicode/ucnv.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace exo {

namespace {

constexpr char kTextPlain[] = "text/plain";
constexpr char kTextRTF[] = "text/rtf";
constexpr char kTextHTML[] = "text/html";
constexpr char kTextUriList[] = "text/uri-list";
constexpr char kApplicationOctetStream[] = "application/octet-stream";
constexpr char kWebCustomData[] = "chromium/x-web-custom-data";
constexpr char kDataTransferEndpoint[] = "chromium/x-data-transfer-endpoint";

constexpr char kUtfPrefix[] = "UTF";
constexpr char kEncoding16[] = "16";
constexpr char kEncodingASCII[] = "ASCII";

constexpr char kUTF16Unspecified[] = "UTF-16";
constexpr char kUTF16LittleEndian[] = "UTF-16LE";
constexpr char kUTF16BigEndian[] = "UTF-16BE";
constexpr uint8_t kByteOrderMark[] = {0xFE, 0xFF};
constexpr int kByteOrderMarkSize = sizeof(kByteOrderMark);

constexpr char kImageBitmap[] = "image/bmp";
constexpr char kImagePNG[] = "image/png";
constexpr char kImageAPNG[] = "image/apng";

std::optional<std::vector<uint8_t>> ReadDataOnWorkerThread(base::ScopedFD fd) {
  constexpr size_t kChunkSize = 1024;
  std::vector<uint8_t> bytes;
  while (true) {
    uint8_t chunk[kChunkSize];
    ssize_t bytes_read = HANDLE_EINTR(read(fd.get(), chunk, kChunkSize));
    if (bytes_read > 0) {
      bytes.insert(bytes.end(), chunk, chunk + bytes_read);
      continue;
    }
    if (!bytes_read)
      return bytes;
    if (bytes_read < 0) {
      PLOG(ERROR) << "Failed to read selection data from clipboard";
      return std::nullopt;
    }
  }
}

// Map a named character set to an integer ranking, lower is better. This is an
// implementation detail of DataSource::GetPreferredMimeTypes and should not be
// considered to have any greater meaning. In particular, these are not expected
// to remain stable over time.
int GetCharsetRank(const std::string& charset_input) {
  std::string charset = base::ToUpperASCII(charset_input);

  // Prefer UTF-16 over all other encodings, because that's what the clipboard
  // interface takes as input; then other unicode encodings; then any non-ASCII
  // encoding, because most or all such encodings are super-sets of ASCII;
  // finally, only use ASCII if nothing else is available.
  if (base::StartsWith(charset, kUtfPrefix, base::CompareCase::SENSITIVE)) {
    if (charset.find(kEncoding16) != std::string::npos)
      return 0;
    return 1;
  } else if (charset.find(kEncodingASCII) == std::string::npos) {
    return 2;
  }
  return 3;
}

// Map an image MIME type to an integer ranking, lower is better. This is an
// implementation detail of DataSource::GetPreferredMimeTypes and should not be
// considered to have any greater meaning. In particular, these are not expected
// to remain stable over time.
int GetImageTypeRank(const std::string& mime_type) {
  // Prefer PNG most of all because this format preserves the alpha channel and
  // is lossless, followed by BMP for being lossless and fast to decode (but
  // doesn't preserve alpha), followed by everything else.
  if (net::MatchesMimeType(std::string(kImagePNG), mime_type) ||
      net::MatchesMimeType(std::string(kImageAPNG), mime_type))
    return 0;
  if (net::MatchesMimeType(std::string(kImageBitmap), mime_type))
    return 1;
  return 2;
}

std::u16string CodepageToUTF16(const std::vector<uint8_t>& data,
                               const std::string& charset_input) {
  std::u16string output;
  std::string_view piece(reinterpret_cast<const char*>(data.data()),
                         data.size());
  const char* charset = charset_input.c_str();

  // Despite claims in the documentation to the contrary, the ICU UTF-16
  // converter does not automatically detect and interpret the byte order
  // mark. Therefore, we must do this ourselves.
  if (!ucnv_compareNames(charset, kUTF16Unspecified) &&
      data.size() >= kByteOrderMarkSize) {
    if (static_cast<uint8_t>(piece.data()[0]) == kByteOrderMark[0] &&
        static_cast<uint8_t>(piece.data()[1]) == kByteOrderMark[1]) {
      // BOM is in big endian format. Consume the BOM so it doesn't get
      // interpreted as a character.
      piece.remove_prefix(2);
      charset = kUTF16BigEndian;
    } else if (static_cast<uint8_t>(piece.data()[0]) == kByteOrderMark[1] &&
               static_cast<uint8_t>(piece.data()[1]) == kByteOrderMark[0]) {
      // BOM is in little endian format. Consume the BOM so it doesn't get
      // interpreted as a character.
      piece.remove_prefix(2);
      charset = kUTF16LittleEndian;
    }
  }

  base::CodepageToUTF16(
      piece, charset, base::OnStringConversionError::Type::SUBSTITUTE, &output);
  return output;
}

// Returns name parameter in application/octet-stream;name=<...>, or empty
// string if parsing fails.
std::string GetApplicationOctetStreamName(const std::string& mime_type) {
  base::StringPairs params;
  if (net::MatchesMimeType(std::string(kApplicationOctetStream), mime_type) &&
      net::ParseMimeType(mime_type, nullptr, &params)) {
    for (const auto& kv : params) {
      if (kv.first == "name")
        return kv.second;
    }
  }
  return std::string();
}

}  // namespace

ScopedDataSource::ScopedDataSource(DataSource* data_source,
                                   DataSourceObserver* observer)
    : data_source_(data_source), observer_(observer) {
  data_source_->AddObserver(observer_);
}

ScopedDataSource::~ScopedDataSource() {
  data_source_->RemoveObserver(observer_);
}

DataSource::DataSource(DataSourceDelegate* delegate)
    : delegate_(delegate), finished_(false) {}

DataSource::~DataSource() {
  delegate_->OnDataSourceDestroying(this);
  for (DataSourceObserver& observer : observers_) {
    observer.OnDataSourceDestroying(this);
  }
}

void DataSource::AddObserver(DataSourceObserver* observer) {
  observers_.AddObserver(observer);
}

void DataSource::RemoveObserver(DataSourceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DataSource::Offer(const std::string& mime_type) {
  mime_types_.insert(mime_type);
}

void DataSource::SetActions(const base::flat_set<DndAction>& dnd_actions) {
  dnd_actions_ = dnd_actions;
}

void DataSource::Target(const std::optional<std::string>& mime_type) {
  delegate_->OnTarget(mime_type);
}

void DataSource::Action(DndAction action) {
  delegate_->OnAction(action);
}

void DataSource::DndDropPerformed() {
  delegate_->OnDndDropPerformed();
}

void DataSource::Cancelled() {
  finished_ = true;
  read_data_weak_ptr_factory_.InvalidateWeakPtrs();
  delegate_->OnCancelled();
}

void DataSource::DndFinished() {
  finished_ = true;
  read_data_weak_ptr_factory_.InvalidateWeakPtrs();
  delegate_->OnDndFinished();
}

std::vector<ui::FileInfo> DataSource::GetFilenames(
    ui::EndpointType source,
    const std::vector<uint8_t>& data) const {
  return delegate_->GetSecurityDelegate()->GetFilenames(source, data);
}

void DataSource::ReadDataForTesting(const std::string& mime_type,
                                    ReadDataCallback callback,
                                    base::RepeatingClosure failure_callback) {
  ReadData(mime_type, std::move(callback), failure_callback);
}

void DataSource::ReadData(const std::string& mime_type,
                          ReadDataCallback callback,
                          base::OnceClosure failure_callback) {
  // This DataSource does not contain the requested MIME type.
  if (mime_type.empty() || !mime_types_.count(mime_type) || finished_) {
    std::move(failure_callback).Run();
    return;
  }

  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  PCHECK(base::CreatePipe(&read_fd, &write_fd));
  delegate_->OnSend(mime_type, std::move(write_fd));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ReadDataOnWorkerThread, std::move(read_fd)),
      base::BindOnce(
          &DataSource::OnDataRead, read_data_weak_ptr_factory_.GetWeakPtr(),
          std::move(callback), mime_type, std::move(failure_callback)));
}

// static
void DataSource::OnDataRead(base::WeakPtr<DataSource> data_source_ptr,
                            ReadDataCallback callback,
                            const std::string& mime_type,
                            base::OnceClosure failure_callback,
                            const std::optional<std::vector<uint8_t>>& data) {
  if (!data_source_ptr || !data) {
    std::move(failure_callback).Run();
    return;
  }
  std::move(callback).Run(mime_type, *data);
}

void DataSource::ReadDataTransferEndpoint(
    ReadTextDataCallback dte_reader,
    base::RepeatingClosure failure_callback) {
  ReadData(kDataTransferEndpoint,
           base::BindOnce(&DataSource::OnTextRead,
                          read_data_weak_ptr_factory_.GetWeakPtr(),
                          std::move(dte_reader)),
           failure_callback);
}

void DataSource::GetDataForPreferredMimeTypes(
    ReadTextDataCallback text_reader,
    ReadDataCallback rtf_reader,
    ReadTextDataCallback html_reader,
    ReadDataCallback image_reader,
    ReadDataCallback filenames_reader,
    ReadFileContentsDataCallback file_contents_reader,
    ReadDataCallback web_custom_data_reader,
    base::RepeatingClosure failure_callback) {
  std::string text_mime;
  std::string rtf_mime;
  std::string html_mime;
  std::string image_mime;
  std::string filenames_mime;
  std::string file_contents_mime;
  std::string web_custom_data_mime;

  int text_rank = std::numeric_limits<int>::max();
  int html_rank = std::numeric_limits<int>::max();
  int image_rank = std::numeric_limits<int>::max();

  for (auto mime_type : mime_types_) {
    if (net::MatchesMimeType(std::string(kTextPlain), mime_type) ||
        mime_type == ui::kMimeTypeLinuxUtf8String) {
      if (text_reader.is_null())
        continue;

      std::string charset;
      charset = GetCharset(mime_type);
      int new_rank = GetCharsetRank(charset);
      if (new_rank < text_rank) {
        text_mime = mime_type;
        text_rank = new_rank;
      }
    } else if (net::MatchesMimeType(std::string(kTextRTF), mime_type)) {
      if (rtf_reader.is_null())
        continue;

      // The RTF MIME type will never have a character set because it only uses
      // 7-bit bytes and stores character set information internally.
      rtf_mime = mime_type;
    } else if (net::MatchesMimeType(std::string(kTextHTML), mime_type)) {
      if (html_reader.is_null())
        continue;

      auto charset = GetCharset(mime_type);
      int new_rank = GetCharsetRank(charset);
      if (new_rank < html_rank) {
        html_mime = mime_type;
        html_rank = new_rank;
      }
    } else if (blink::IsSupportedImageMimeType(mime_type)) {
      if (image_reader.is_null())
        continue;

      int new_rank = GetImageTypeRank(mime_type);
      if (new_rank < image_rank) {
        image_mime = mime_type;
        image_rank = new_rank;
      }
    } else if (net::MatchesMimeType(std::string(kTextUriList), mime_type)) {
      if (filenames_reader.is_null())
        continue;

      filenames_mime = mime_type;
    } else if (!GetApplicationOctetStreamName(mime_type).empty()) {
      file_contents_mime = mime_type;
    } else if (net::MatchesMimeType(std::string(kWebCustomData), mime_type)) {
      web_custom_data_mime = mime_type;
    }
  }

  ReadData(text_mime,
           base::BindOnce(&DataSource::OnTextRead,
                          read_data_weak_ptr_factory_.GetWeakPtr(),
                          std::move(text_reader)),
           failure_callback);
  ReadData(rtf_mime, std::move(rtf_reader), failure_callback);
  ReadData(html_mime,
           base::BindOnce(&DataSource::OnTextRead,
                          read_data_weak_ptr_factory_.GetWeakPtr(),
                          std::move(html_reader)),
           failure_callback);
  ReadData(image_mime, std::move(image_reader), failure_callback);
  ReadData(filenames_mime, std::move(filenames_reader), failure_callback);
  ReadData(file_contents_mime,
           base::BindOnce(&DataSource::OnFileContentsRead,
                          read_data_weak_ptr_factory_.GetWeakPtr(),
                          std::move(file_contents_reader)),
           failure_callback);
  ReadData(web_custom_data_mime, std::move(web_custom_data_reader),
           failure_callback);
}

void DataSource::OnTextRead(ReadTextDataCallback callback,
                            const std::string& mime_type,
                            const std::vector<uint8_t>& data) {
  std::u16string output = CodepageToUTF16(data, GetCharset(mime_type));
  std::move(callback).Run(mime_type, std::move(output));
}

void DataSource::OnFileContentsRead(ReadFileContentsDataCallback callback,
                                    const std::string& mime_type,
                                    const std::vector<uint8_t>& data) {
  const base::FilePath filename(GetApplicationOctetStreamName(mime_type));
  std::move(callback).Run(mime_type, filename, data);
}

bool DataSource::CanBeDataSourceForCopy(Surface* surface) const {
  return delegate_->CanAcceptDataEventsForSurface(surface);
}

}  // namespace exo
