// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/printing/ppd_line_reader.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_util.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/filter/gzip_header.h"
#include "net/filter/gzip_source_stream.h"
#include "net/filter/source_stream.h"

namespace chromeos {
namespace {

constexpr char kPPDMagicNumberString[] = "*PPD-Adobe:";

// Return true if contents has a valid Gzip header.
bool IsGZipped(const std::string& contents) {
  const char* unused;
  return net::GZipHeader().ReadMore(contents.data(), contents.size(),
                                    &unused) ==
         net::GZipHeader::COMPLETE_HEADER;
}

// Return true if c is a newline in the ppd sense, that is, either newline or
// carriage return.
bool IsNewline(char c) {
  return c == '\n' || c == '\r';
}

// Source stream that reads from a string.  A reference is taken to the string
// used by StringSourceStream; it must not be modified while the
// StringSourceStream exists.
class StringSourceStream : public net::SourceStream {
 public:
  explicit StringSourceStream(const std::string& src)
      : SourceStream(TYPE_UNKNOWN), src_(src) {}

  // This source always reads sychronously, so never uses the callback.
  int Read(net::IOBuffer* dest_buffer,
           int buffer_size,
           net::CompletionOnceCallback) override {
    if (buffer_size < 0)
      return net::ERR_INVALID_ARGUMENT;
    if (!MayHaveMoreBytes())
      return net::OK;
    const size_t read_size =
        std::min(src_->size() - read_ofs_, static_cast<size_t>(buffer_size));
    memcpy(dest_buffer->data(), src_->data() + read_ofs_, read_size);
    read_ofs_ += read_size;
    return read_size;
  }
  std::string Description() const override { return ""; }
  bool MayHaveMoreBytes() const override { return read_ofs_ < src_->size(); }

 private:
  size_t read_ofs_ = 0;
  const raw_ref<const std::string> src_;
};

class PpdLineReaderImpl : public PpdLineReader {
 public:
  PpdLineReaderImpl(const std::string& ppd_contents, size_t max_line_length)
      : max_line_length_(max_line_length),
        read_buf_(
            base::MakeRefCounted<net::IOBufferWithSize>(kReadBufCapacity)) {
    input_ = std::make_unique<StringSourceStream>(ppd_contents);
    if (IsGZipped(ppd_contents)) {
      input_ = net::GzipSourceStream::Create(std::move(input_),
                                             net::SourceStream::TYPE_GZIP);
    }
  }
  ~PpdLineReaderImpl() override = default;

  bool NextLine(std::string* line) override {
    line->reserve(max_line_length_);

    // Outer loop controls retries; if we fail to read a line, we'll try again
    // after the next newline.
    while (true) {
      line->clear();
      while (line->size() <= max_line_length_) {
        char c = NextChar();
        if (Eof()) {
          return !line->empty();
        } else if (IsNewline(c)) {
          return true;
        }
        line->push_back(c);
      }

      // Exceeded max line length, skip the rest of this line, try for another
      // one.
      if (!SkipToNextLine()) {
        return false;
      }
    }
  }

  std::string RemainingContent() override {
    std::string content(read_buf_->data() + read_ofs_,
                        read_buf_->data() + read_buf_size_);
    for (ReadNextChunk(); read_buf_size_ > 0; ReadNextChunk()) {
      content.append(read_buf_->data(), read_buf_size_);
    }
    return content;
  }

  bool Error() const override { return error_; }

 private:
  // Chunk size of reads to the underlying source stream.
  static constexpr int kReadBufCapacity = 1024;

  // Skip input until we hit a newline (which is discarded).  If
  // we encounter eof before a newline, false is returned.
  bool SkipToNextLine() {
    while (true) {
      char c = NextChar();
      if (Eof()) {
        return false;
      }
      if (IsNewline(c)) {
        return true;
      }
    }
  }

  void ReadNextChunk() {
    // Just ignore if we already reach EOF.
    if (eof_) {
      return;
    }
    read_ofs_ = 0;

    // Since StringSourceStream never uses the callback, and filter streams
    // are only supposed to use the callback if the underlying source stream
    // uses it, we should never see the callback used.
    int result = input_->Read(
        read_buf_.get(), kReadBufCapacity,
        base::BindOnce([](int) { LOG(FATAL) << "Unexpected async read"; }));
    if (result <= 0) {
      eof_ = true;
      error_ = (result < 0);
      read_buf_size_ = 0;
    } else {
      read_buf_size_ = result;
    }
  }

  // Consume and return the next char from the source stream.  If there is no
  // more data to be had, set eof.  Eof() should be checked before the returned
  // value is used.
  char NextChar() {
    if (read_ofs_ == read_buf_size_) {
      // Grab more data from the underlying stream.
      ReadNextChunk();
      if (read_buf_size_ == 0) {
        return '\0';
      }
    }
    return read_buf_->data()[read_ofs_++];
  }

  bool Eof() const { return eof_; }

  // Maximum allowable line length from the source.  Any lines longer than this
  // will be silently discarded.
  size_t max_line_length_;

  // Buffer for reading from the source stream.
  scoped_refptr<net::IOBuffer> read_buf_;
  // Number of bytes actually in the buffer.
  int read_buf_size_ = 0;
  // Offset into read_buf for the next char.
  int read_ofs_ = 0;

  // Have we hit the end of the source stream?
  bool eof_ = false;

  // Did we encounter an error while reading?
  bool error_ = false;

  // The input stream we're reading bytes from.  This may be a gzip source
  // stream or string source stream depending on the source data.
  std::unique_ptr<net::SourceStream> input_;
};

constexpr int PpdLineReaderImpl::kReadBufCapacity;

}  // namespace

// static
std::unique_ptr<PpdLineReader> PpdLineReader::Create(
    const std::string& contents,
    size_t max_line_length) {
  return std::make_unique<PpdLineReaderImpl>(contents, max_line_length);
}

// static
bool PpdLineReader::ContainsMagicNumber(const std::string& contents,
                                        size_t max_line_length) {
  auto line_reader = PpdLineReader::Create(contents, max_line_length);
  std::string line;
  return line_reader->NextLine(&line) &&
         base::StartsWith(line, kPPDMagicNumberString,
                          base::CompareCase::SENSITIVE);
}

}  // namespace chromeos
