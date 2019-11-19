// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOGIN_LOCALIZED_VALUES_BUILDER_H_
#define COMPONENTS_LOGIN_LOCALIZED_VALUES_BUILDER_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/login/login_export.h"

namespace base {
class DictionaryValue;
}

namespace login {

// Class that collects Localized Values for translation.
class LOGIN_EXPORT LocalizedValuesBuilder {
 public:
  explicit LocalizedValuesBuilder(base::DictionaryValue* dict);
  explicit LocalizedValuesBuilder(const std::string& prefix,
                                  base::DictionaryValue* dict);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message| is text of the message.
  void Add(const std::string& key, const std::string& message);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message| is text of the message.
  void Add(const std::string& key, const base::string16& message);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message.
  void Add(const std::string& key, int message_id);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // one format parameter subsituted by |a|.
  void AddF(const std::string& key, int message_id, const base::string16& a);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // two format parameters subsituted by |a| and |b| respectively.
  void AddF(const std::string& key,
            int message_id,
            const base::string16& a,
            const base::string16& b);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // two format parameters subsituted by |a|, |b| and |c| respectively.
  void AddF(const std::string& key,
            int message_id,
            const base::string16& a,
            const base::string16& b,
            const base::string16& c);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // one format parameter subsituted by resource identified by |message_id_a|.
  void AddF(const std::string& key, int message_id, int message_id_a);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // two format parameters subsituted by resource identified by |message_id_a|
  // and |message_id_b| respectively.
  void AddF(const std::string& key,
            int message_id,
            int message_id_a,
            int message_id_b);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // three format parameters subsituted by resource identified by
  // |message_id_a|, |message_id_b| and |message_id_c| respectively.
  void AddF(const std::string& key,
            int message_id,
            int message_id_a,
            int message_id_b,
            int message_id_c);

 private:
  std::string prefix_;

  // Not owned.
  base::DictionaryValue* dict_;
};

}  // namespace login

#endif  // COMPONENTS_LOGIN_LOCALIZED_VALUES_BUILDER_H_
