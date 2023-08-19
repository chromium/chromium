// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_PREFS_IMPL_H_
#define CHROME_UPDATER_PREFS_IMPL_H_

#include <memory>
#include <string>

#include "chrome/updater/lock.h"
#include "chrome/updater/prefs.h"

namespace base {
class FilePath;
}

namespace updater {

enum class UpdaterScope;

class UpdaterPrefsImpl : public LocalPrefs, public GlobalPrefs {
 public:
  UpdaterPrefsImpl(const base::FilePath& prefs_dir_,
                   std::unique_ptr<ScopedLock> lock,
                   std::unique_ptr<PrefService> prefs);

  // Overrides for UpdaterPrefs.
  PrefService* GetPrefService() const override;

  // Overrides for LocalPrefs
  bool GetQualified() const override;
  void SetQualified(bool value) override;

  // Overrides for GlobalPrefs
  std::string GetActiveVersion() const override;
  void SetActiveVersion(const std::string& value) override;
  bool GetSwapping() const override;
  void SetSwapping(bool value) override;
  bool GetMigratedLegacyUpdaters() const override;
  void SetMigratedLegacyUpdaters() override;
  int CountServerStarts() override;

 protected:
  ~UpdaterPrefsImpl() override;

 private:
  // `prefs_dir_` is used for logging purposes and it may be deprecated later.
  const base::FilePath prefs_dir_;
  std::unique_ptr<ScopedLock> lock_;
  std::unique_ptr<PrefService> prefs_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_PREFS_IMPL_H_
