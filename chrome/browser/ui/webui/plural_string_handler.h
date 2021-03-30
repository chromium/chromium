// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PLURAL_STRING_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PLURAL_STRING_HANDLER_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_message_handler.h"

// A handler which provides pluralized strings.
class PluralStringHandler : public content::WebUIMessageHandler {
 public:
  PluralStringHandler();
  ~PluralStringHandler() override;

  void AddLocalizedString(const std::string& name, int id);

  // WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleGetPluralString(const base::ListValue* args);

  // Constructs two pluralized strings from the received arguments for the two
  // strings, and then concatenates those with comma and whitespace in between.
  void HandleGetPluralStringTupleWithComma(const base::ListValue* args);

  // Constructs two pluralized strings from the received arguments for the two
  // strings, and then concatenates those with period and whitespace in between,
  // and a period afterwards.
  void HandleGetPluralStringTupleWithPeriods(const base::ListValue* args);

  // Constructs two pluralized strings from the received arguments for the two
  // strings, and then concatenates those using the concatenation template
  // specified. This method should only be called from within the
  // |HandleGetPluralStringTuple*| methods above.
  void GetPluralStringTuple(const base::ListValue* args, int string_tuple_id);

  std::u16string GetPluralizedStringForMessageName(std::string message_name,
                                                   int count);

  std::map<std::string, int> name_to_id_;

  DISALLOW_COPY_AND_ASSIGN(PluralStringHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PLURAL_STRING_HANDLER_H_
