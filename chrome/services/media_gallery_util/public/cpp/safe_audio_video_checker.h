// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_CPP_SAFE_AUDIO_VIDEO_CHECKER_H_
#define CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_CPP_SAFE_AUDIO_VIDEO_CHECKER_H_

#include "base/files/file.h"
#include "chrome/services/media_gallery_util/public/cpp/media_parser_provider.h"

// Uses a utility process to validate a media file.  If the callback returns
// File::FILE_OK, then file appears to be valid.  File validation does not
// attempt to decode the entire file since that could take a considerable
// amount of time.
class SafeAudioVideoChecker : public MediaParserProvider {
 public:
  using ResultCallback = base::OnceCallback<void(base::File::Error result)>;

  // Takes responsibility for closing |file|.
  SafeAudioVideoChecker(base::File file, ResultCallback callback);

  SafeAudioVideoChecker(const SafeAudioVideoChecker&) = delete;
  SafeAudioVideoChecker& operator=(const SafeAudioVideoChecker&) = delete;

  ~SafeAudioVideoChecker() override;

  // Checks the file. Can be called on a different thread than the UI thread.
  // Note that the callback specified in the constructor will be called on the
  // thread from which this method is called.
  void Start();

 private:
  // MediaParserProvider implementation:
  void OnMediaParserCreated() override;
  void OnConnectionError() override;

  // Media file check result.
  void CheckMediaFileDone(bool valid);

  // Media file to check.
  base::File file_;

  // Report the check result to |callback_|.
  ResultCallback callback_;
};

#endif  // CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_CPP_SAFE_AUDIO_VIDEO_CHECKER_H_
