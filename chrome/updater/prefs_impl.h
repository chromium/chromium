// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_PREFS_IMPL_H_
#define CHROME_UPDATER_PREFS_IMPL_H_

#include <memory>
#include <string>

#include "chrome/updater/prefs.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace updater {

enum class UpdaterScope;
class ScopedPrefsLockImpl;

// ScopedPrefsLock represents a held lock. Destroying the ScopedPrefsLock
// releases the lock. Implementors cannot depend on a ScopedPrefsLock being
// reentrant. The definition of ScopedPrefsLockImpl is platform-specific.
class ScopedPrefsLock {
 public:
  explicit ScopedPrefsLock(std::unique_ptr<ScopedPrefsLockImpl> impl);
  ScopedPrefsLock(const ScopedPrefsLock&) = delete;
  ScopedPrefsLock& operator=(const ScopedPrefsLock&) = delete;
  ~ScopedPrefsLock();

 private:
  std::unique_ptr<ScopedPrefsLockImpl> impl_;
};

class UpdaterPrefsImpl : public LocalPrefs, public GlobalPrefs {
 public:
  UpdaterPrefsImpl(std::unique_ptr<ScopedPrefsLock> lock,
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
  std::unique_ptr<ScopedPrefsLock> lock_;
  std::unique_ptr<PrefService> prefs_;
};

// Returns a ScopedPrefsLock, or nullptr if the lock could not be acquired
// within the timeout. While the ScopedPrefsLock exists, no other process on
// the machine may access global prefs.
std::unique_ptr<ScopedPrefsLock> AcquireGlobalPrefsLock(
    UpdaterScope scope,
    base::TimeDelta timeout);

}  // namespace updater

#endif  // CHROME_UPDATER_PREFS_IMPL_H_
