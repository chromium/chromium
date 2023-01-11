// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_SERVICE_FACTORY_H_
#define COMPONENTS_PREFS_PREF_SERVICE_FACTORY_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/pref_value_store.h"
#include "components/prefs/prefs_export.h"

class PrefService;

namespace base {
class FilePath;
class SequencedTaskRunner;
}

// A class that allows convenient building of PrefService.
class COMPONENTS_PREFS_EXPORT PrefServiceFactory {
 public:
  PrefServiceFactory();

  PrefServiceFactory(const PrefServiceFactory&) = delete;
  PrefServiceFactory& operator=(const PrefServiceFactory&) = delete;

  virtual ~PrefServiceFactory();

  // Functions for setting the various parameters of the PrefService to build.
  void set_managed_prefs(scoped_refptr<PrefStore> prefs) {
    managed_prefs_.swap(prefs);
  }

  void set_supervised_user_prefs(scoped_refptr<PrefStore> prefs) {
    supervised_user_prefs_.swap(prefs);
  }

  void set_extension_prefs(scoped_refptr<PrefStore> prefs) {
    extension_prefs_.swap(prefs);
  }

  void set_standalone_browser_prefs(scoped_refptr<PersistentPrefStore> prefs) {
    standalone_browser_prefs_.swap(prefs);
  }

  void set_command_line_prefs(scoped_refptr<PrefStore> prefs) {
    command_line_prefs_.swap(prefs);
  }

  void set_user_prefs(scoped_refptr<PersistentPrefStore> prefs) {
    user_prefs_.swap(prefs);
  }

  void set_recommended_prefs(scoped_refptr<PrefStore> prefs) {
    recommended_prefs_.swap(prefs);
  }

  // Sets up error callback for the PrefService.  A do-nothing default is
  // provided if this is not called. This callback is always invoked (async or
  // not) on the sequence on which Create is invoked.
  void set_read_error_callback(
      base::RepeatingCallback<void(PersistentPrefStore::PrefReadError)>
          read_error_callback) {
    read_error_callback_ = std::move(read_error_callback);
  }

  // Specifies to use an actual file-backed user pref store.
  void SetUserPrefsFile(const base::FilePath& prefs_file,
                        base::SequencedTaskRunner* task_runner);

  void set_async(bool async) {
    async_ = async;
  }

  // Creates a PrefService object initialized with the parameters from
  // this factory.
  std::unique_ptr<PrefService> Create(
      scoped_refptr<PrefRegistry> pref_registry);

 protected:
  scoped_refptr<PrefStore> managed_prefs_;
  scoped_refptr<PrefStore> supervised_user_prefs_;
  scoped_refptr<PrefStore> extension_prefs_;
  scoped_refptr<PersistentPrefStore> standalone_browser_prefs_;
  scoped_refptr<PrefStore> command_line_prefs_;
  scoped_refptr<PersistentPrefStore> user_prefs_;
  scoped_refptr<PrefStore> recommended_prefs_;

  base::RepeatingCallback<void(PersistentPrefStore::PrefReadError)>
      read_error_callback_;

  // Defaults to false.
  bool async_;
};

#endif  // COMPONENTS_PREFS_PREF_SERVICE_FACTORY_H_
