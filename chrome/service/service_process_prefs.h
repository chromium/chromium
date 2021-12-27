// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_SERVICE_PROCESS_PREFS_H_
#define CHROME_SERVICE_SERVICE_PROCESS_PREFS_H_

#include <memory>
#include <string>

#include "components/prefs/json_pref_store.h"

namespace base {
class DictionaryValue;
class SequencedTaskRunner;
}

// Manages persistent preferences for the service process. This is basically a
// thin wrapper around JsonPrefStore for more comfortable use.
class ServiceProcessPrefs {
 public:
  // |sequenced_task_runner| must be a shutdown-blocking task runner.
  ServiceProcessPrefs(const base::FilePath& pref_filename,
                      base::SequencedTaskRunner* task_runner);

  ServiceProcessPrefs(const ServiceProcessPrefs&) = delete;
  ServiceProcessPrefs& operator=(const ServiceProcessPrefs&) = delete;

  ~ServiceProcessPrefs();

  // Read preferences from the backing file.
  void ReadPrefs();

  // Write the data to the backing file.
  void WritePrefs();

  // Returns a string preference for |key|.
  std::string GetString(const std::string& key,
                        const std::string& default_value) const;

  // Set a string |value| for |key|.
  void SetString(const std::string& key, const std::string& value);

  // Returns a boolean preference for |key|.
  bool GetBoolean(const std::string& key, bool default_value) const;

  // Set a boolean |value| for |key|.
  void SetBoolean(const std::string& key, bool value);

  // Returns an int preference for |key|.
  int GetInt(const std::string& key, int default_value) const;

  // Set an int |value| for |key|.
  void SetInt(const std::string& key, int value);

  // Returns a dictionary preference for |key|.
  const base::DictionaryValue* GetDictionary(const std::string& key) const;

  // Returns a list for |key|.
  const base::Value* GetList(const std::string& key) const;

  // Set a |value| for |key|.
  void SetValue(const std::string& key, std::unique_ptr<base::Value> value);

  // Removes the pref specified by |key|.
  void RemovePref(const std::string& key);

 private:
  scoped_refptr<JsonPrefStore> prefs_;
};

#endif  // CHROME_SERVICE_SERVICE_PROCESS_PREFS_H_
