// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_content.h"

#include "base/notreached.h"

namespace download {

std::string GetDownloadContentString(const DownloadContent& download_content) {
  switch (download_content) {
    case DownloadContent::UNRECOGNIZED:
      return "UNRECOGNIZED";
    case DownloadContent::TEXT:
      return "TEXT";
    case DownloadContent::IMAGE:
      return "IMAGE";
    case DownloadContent::AUDIO:
      return "AUDIO";
    case DownloadContent::VIDEO:
      return "VIDEO";
    case DownloadContent::OCTET_STREAM:
      return "OCTET_STREAM";
    case DownloadContent::PDF:
      return "PDF";
    case DownloadContent::DOCUMENT:
      return "DOCUMENT";
    case DownloadContent::SPREADSHEET:
      return "SPREADSHEET";
    case DownloadContent::PRESENTATION:
      return "PRESENTATION";
    case DownloadContent::ARCHIVE:
      return "ARCHIVE";
    case DownloadContent::EXECUTABLE:
      return "EXECUTABLE";
    case DownloadContent::DMG:
      return "DMG";
    case DownloadContent::CRX:
      return "CRX";
    case DownloadContent::WEB:
      return "WEB";
    case DownloadContent::EBOOK:
      return "EBOOK";
    case DownloadContent::FONT:
      return "FONT";
    case DownloadContent::APK:
      return "APK";
    case DownloadContent::MAX:
      return "MAX";
  }
  NOTREACHED();
  return "";
}

}  // namespace download
