// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_ENTRY_H__
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_ENTRY_H__

#include <string>

#include "base/time/time.h"

namespace autofill {

class AutofillKey {
 public:
  AutofillKey();
  AutofillKey(const std::u16string& name, const std::u16string& value);
  AutofillKey(const std::string& name, const std::string& value);
  AutofillKey(const AutofillKey& key);
  virtual ~AutofillKey();

  const std::u16string& name() const { return name_; }
  const std::u16string& value() const { return value_; }

  bool operator==(const AutofillKey& key) const;
  bool operator<(const AutofillKey& key) const;

 private:
  std::u16string name_;
  std::u16string value_;
};

class AutofillEntry {
 public:
  AutofillEntry();
  AutofillEntry(const AutofillKey& key,
                const base::Time& date_created,
                const base::Time& date_last_used);
  ~AutofillEntry();

  const AutofillKey& key() const { return key_; }
  const base::Time& date_created() const { return date_created_; }
  const base::Time& date_last_used() const { return date_last_used_; }

  bool operator==(const AutofillEntry& entry) const;
  bool operator!=(const AutofillEntry& entry) const;
  bool operator<(const AutofillEntry& entry) const;

 private:
  AutofillKey key_;
  base::Time date_created_;
  base::Time date_last_used_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_ENTRY_H__
