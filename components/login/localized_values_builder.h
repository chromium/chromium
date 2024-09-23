// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOGIN_LOCALIZED_VALUES_BUILDER_H_
#define COMPONENTS_LOGIN_LOCALIZED_VALUES_BUILDER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/login/login_export.h"

namespace login {

// Class that collects Localized Values for translation.
class LOGIN_EXPORT LocalizedValuesBuilder {
 public:
  explicit LocalizedValuesBuilder(base::Value::Dict* dict);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message| is text of the message.
  void Add(const std::string& key, const std::string& message);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message| is text of the message.
  void Add(const std::string& key, const std::u16string& message);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message.
  void Add(const std::string& key, int message_id);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // one format parameter subsituted by |a|.
  void AddF(const std::string& key, int message_id, const std::u16string& a);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // two format parameters subsituted by |a| and |b| respectively.
  void AddF(const std::string& key,
            int message_id,
            const std::u16string& a,
            const std::u16string& b);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // two format parameters subsituted by |a|, |b| and |c| respectively.
  void AddF(const std::string& key,
            int message_id,
            const std::u16string& a,
            const std::u16string& b,
            const std::u16string& c);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // one format parameter subsituted by resource identified by |message_id_a|.
  void AddF(const std::string& key, int message_id, int message_id_a);

 private:
  // Not owned.
  raw_ptr<base::Value::Dict> dict_;
};

}  // namespace login

#endif  // COMPONENTS_LOGIN_LOCALIZED_VALUES_BUILDER_H_
