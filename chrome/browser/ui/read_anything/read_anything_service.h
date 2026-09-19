// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_SERVICE_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_SERVICE_H_

#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

// This per-profile class holds profile-scoped state for the read anything
// feature.
class ReadAnythingService : public KeyedService {
 public:
  ReadAnythingService();
  ~ReadAnythingService() override;

  static ReadAnythingService* Get(Profile* profile);
#if !BUILDFLAG(IS_CHROMEOS)
  static void InstallComponent(const base::FilePath& new_dir);
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // Called by the per-tab ReadAnythingSidePanelController and in
  // ReadAnythingController.
  virtual void OnReadAnythingShown();

 private:
  void RemoveTtsDownloadExtension();
  static void RecordEngineVersion(const base::FilePath& engine_version);
#if !BUILDFLAG(IS_CHROMEOS)
  void SetupDesktopEngine();
#endif  // !BUILDFLAG(IS_CHROMEOS)
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_SERVICE_H_
