// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_THIRD_PARTY_METRICS_RECORDER_H_
#define CHROME_BROWSER_WIN_CONFLICTS_THIRD_PARTY_METRICS_RECORDER_H_

#include <string>

#include "build/branding_buildflags.h"
#include "chrome/browser/win/conflicts/module_database_observer.h"

struct ModuleInfoKey;

// Records metrics about third party modules loaded into Chrome.
class ThirdPartyMetricsRecorder : public ModuleDatabaseObserver {
 public:
  ThirdPartyMetricsRecorder();

  ThirdPartyMetricsRecorder(const ThirdPartyMetricsRecorder&) = delete;
  ThirdPartyMetricsRecorder& operator=(const ThirdPartyMetricsRecorder&) =
      delete;

  ~ThirdPartyMetricsRecorder() override;

  // ModuleDatabaseObserver:
  void OnNewModuleFound(const ModuleInfoKey& module_key,
                        const ModuleInfoData& module_data) override;
  void OnModuleDatabaseIdle() override;

 private:
  // The size of the unsigned modules crash keys.
  static constexpr size_t kCrashKeySize = 256;

  // A helper function that writes the unsigned modules name to a series of
  // crash keys. The crash keys are leaked so that they can be picked up by the
  // crash reporter. Creating another instance of ThirdPartyMetricsRecorder
  // will start overwriting the current values in the crash keys. This is not
  // a problem in practice because this class is leaked.
  void AddUnsignedModuleToCrashkeys(const std::wstring& module_basename);

  // The index of the crash key that is currently being updated.
  size_t current_key_index_ = 0;

  // The value of the crash key that is currently being updated.
  std::string current_value_;
};

#endif  // CHROME_BROWSER_WIN_CONFLICTS_THIRD_PARTY_METRICS_RECORDER_H_
