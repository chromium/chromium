// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_CONTENT_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_CONTENT_H_

// The type of download based on mimetype.
// This is used by UMA and UKM metrics.
namespace download {

// NOTE: Keep in sync with DownloadContentType in
// tools/metrics/histograms/enums.xml.
enum class DownloadContent {
  kUnrecognized = 0,
  kText = 1,
  kImage = 2,
  kAudio = 3,
  kVideo = 4,
  kOctetStream = 5,
  kPdf = 6,
  kDocument = 7,
  kSpreadSheet = 8,
  kPresentation = 9,
  kArchive = 10,
  kExecutable = 11,
  kDmg = 12,
  kCrx = 13,
  kWeb = 14,
  kEbook = 15,
  kFont = 16,
  kApk = 17,
  kMaxValue = kApk,
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_CONTENT_H_
