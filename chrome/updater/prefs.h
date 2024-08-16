// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_PREFS_H_
#define CHROME_UPDATER_PREFS_H_

#include <string>

#include "base/functional/function_ref.h"
#include "base/memory/ref_counted.h"
#include "chrome/updater/util/util.h"

class PrefService;

namespace updater {

enum class UpdaterScope;

extern const char kPrefUpdateTime[];

class UpdaterPrefs : public base::RefCountedThreadSafe<UpdaterPrefs> {
 public:
  UpdaterPrefs() = default;
  UpdaterPrefs(const UpdaterPrefs&) = delete;
  UpdaterPrefs& operator=(const UpdaterPrefs&) = delete;

  virtual PrefService* GetPrefService() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<UpdaterPrefs>;
  virtual ~UpdaterPrefs() = default;
};

class LocalPrefs : virtual public UpdaterPrefs {
 public:
  LocalPrefs() = default;

  virtual bool GetQualified() const = 0;
  virtual void SetQualified(bool value) = 0;

 protected:
  ~LocalPrefs() override = default;
};

class GlobalPrefs : virtual public UpdaterPrefs {
 public:
  GlobalPrefs() = default;

  virtual std::string GetActiveVersion() const = 0;
  virtual void SetActiveVersion(const std::string& value) = 0;
  virtual bool GetSwapping() const = 0;
  virtual void SetSwapping(bool value) = 0;
  virtual bool GetMigratedLegacyUpdaters() const = 0;
  virtual void SetMigratedLegacyUpdaters() = 0;

  // The server starts counter is a global pref value that counts the number of
  // active server starts for the updater. If there are no apps registered by
  // the time that this counter exceeds the max number of starts before
  // registration, then the updater will uninstall itself as it is seemingly not
  // being used. The purpose of this value is to prevent the updater from
  // lingering forever after install if no registration takes place.
  virtual int CountServerStarts() = 0;

 protected:
  ~GlobalPrefs() override = default;
};

// Open the global prefs. These prefs are protected by a mutex, and shared by
// all updaters on the system. Returns nullptr if the mutex cannot be acquired.
scoped_refptr<GlobalPrefs> CreateGlobalPrefs(UpdaterScope scope);

// Similar to `CreateGlobalPrefs`, but bypasses the `WrongUser` check for tests.
scoped_refptr<GlobalPrefs> CreateGlobalPrefsForTesting(UpdaterScope scope);

// Open the version-specific prefs. These prefs are not protected by any mutex
// and not shared with other versions of the updater.
scoped_refptr<LocalPrefs> CreateLocalPrefs(UpdaterScope scope);

// Commits prefs changes to storage. This function should only be called
// when the changes must be written immediately, for instance, during program
// shutdown. The function must be called in the scope of a task executor.
void PrefsCommitPendingWrites(PrefService* pref_service);

}  // namespace updater

#endif  // CHROME_UPDATER_PREFS_H_
