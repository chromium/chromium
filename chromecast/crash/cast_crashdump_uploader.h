// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_CAST_CRASHDUMP_UPLOADER_H_
#define CHROMECAST_CRASH_CAST_CRASHDUMP_UPLOADER_H_

#include <map>
#include <memory>
#include <string>

namespace chromecast {
class LibcurlWrapper;

struct CastCrashdumpData {
  CastCrashdumpData();
  CastCrashdumpData(const CastCrashdumpData& other);
  ~CastCrashdumpData();

  std::string product;
  std::string version;
  std::string guid;
  std::string ptime;
  std::string ctime;
  std::string email;
  std::string comments;
  std::string minidump_pathname;
  std::string crash_server;
  std::string proxy_host;
  std::string proxy_userpassword;
  std::string upload_filename;
};

class CastCrashdumpUploader {
 public:
  CastCrashdumpUploader(const CastCrashdumpData& data,
                        std::unique_ptr<LibcurlWrapper> http_layer);
  explicit CastCrashdumpUploader(const CastCrashdumpData& data);

  CastCrashdumpUploader(const CastCrashdumpUploader&) = delete;
  CastCrashdumpUploader& operator=(const CastCrashdumpUploader&) = delete;

  virtual ~CastCrashdumpUploader();

  virtual bool AddAttachment(const std::string& label,
                             const std::string& filename);
  virtual void SetParameter(const std::string& key, const std::string& value);
  virtual bool Upload(std::string* response);

 private:
  bool CheckRequiredParametersArePresent();

  std::unique_ptr<LibcurlWrapper> http_layer_;
  CastCrashdumpData data_;

  // Holds the following mapping for attachments: <label, filepath>
  std::map<std::string, std::string> attachments_;

  // Holds the following mapping for HTTP request params: <key, value>
  std::map<std::string, std::string> parameters_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_CAST_CRASHDUMP_UPLOADER_H_
