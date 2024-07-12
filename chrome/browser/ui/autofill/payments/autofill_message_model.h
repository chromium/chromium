// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_MESSAGE_MODEL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_MESSAGE_MODEL_H_

#include <memory>
#include <string>

#include "base/types/pass_key.h"
#include "components/messages/android/message_wrapper.h"

namespace autofill {

class AutofillMessageController;

// AutofillMessageModel is used to create autofill Android Messages to be used
// with the AutofillMessageController.
class AutofillMessageModel {
 public:
  // The type of autofill message model.
  enum class Type {
    // Unspecified message model type.
    kUnspecified = 0,
    // Used when a card has failed to have been saved to the server.
    kSaveCardFailure = 1,
    // Used when a virtual card has failed to have been enrolled.
    kVirtualCardEnrollFailure = 2,
  };

  AutofillMessageModel() = delete;
  ~AutofillMessageModel();

  AutofillMessageModel(const AutofillMessageModel&) = delete;
  AutofillMessageModel& operator=(const AutofillMessageModel&) = delete;

  AutofillMessageModel(AutofillMessageModel&&);
  AutofillMessageModel& operator=(AutofillMessageModel&&);

  static std::unique_ptr<AutofillMessageModel> CreateForSaveCardFailure();
  static std::unique_ptr<AutofillMessageModel>
  CreateForVirtualCardEnrollFailure(std::u16string card_label);

  messages::MessageWrapper& GetMessage(
      base::PassKey<AutofillMessageController> pass_key);
  const Type& GetType() const;
  std::string_view GetTypeAsString() const;

 private:
  friend class AutofillMessageModelTest;

  // Only used by the factory functions.
  AutofillMessageModel(std::unique_ptr<messages::MessageWrapper> message,
                       Type type);

  // Converts a message model type to a string for debugging and metrics.
  static std::string_view TypeToString(Type message_type);

  // The message contains the Android message (populated with the appropriate
  // text and icons) which can be used with the MessageDispatcherBridge.
  std::unique_ptr<messages::MessageWrapper> message_;
  // The type represents the type of Autofill message this is.
  Type type_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_MESSAGE_MODEL_H_
