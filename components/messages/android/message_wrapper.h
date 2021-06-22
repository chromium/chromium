// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_MESSAGE_WRAPPER_H_
#define COMPONENTS_MESSAGES_ANDROID_MESSAGE_WRAPPER_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "components/messages/android/message_enums.h"

namespace messages {

// |MessagesWrapper| represents a message for native feature code. It accepts
// callbacks for action and dismiss events and provides methods for setting
// message properties. After setting message's properties feature code can
// enqueue the message through |MessageDispatcherBridge|.
class MessageWrapper {
 public:
  using DismissCallback = base::OnceCallback<void(DismissReason)>;

  MessageWrapper(MessageIdentifier message_identifier,
                 base::OnceClosure action_callback,
                 DismissCallback dismiss_callback);
  ~MessageWrapper();

  MessageWrapper(const MessageWrapper&) = delete;
  MessageWrapper& operator=(const MessageWrapper&) = delete;

  // Methods to manipulate message properties. On Android the values are
  // propagated to Java and stored in |PropertyModel|.
  std::u16string GetTitle();
  void SetTitle(const std::u16string& title);
  std::u16string GetDescription();
  void SetDescription(const std::u16string& description);

  // SetDescriptionMaxLines allows limiting description view to the specified
  // number of lines. The description will be ellipsized with TruncateAt.END
  // option.
  int GetDescriptionMaxLines();
  void SetDescriptionMaxLines(int max_lines);
  std::u16string GetPrimaryButtonText();
  void SetPrimaryButtonText(const std::u16string& primary_button_text);
  std::u16string GetSecondaryButtonMenuText();
  void SetSecondaryButtonMenuText(
      const std::u16string& secondary_button_menu_text);

  // When setting a message icon use ResourceMapper::MapToJavaDrawableId to
  // translate from chromium resource_id to Android drawable resource_id.
  int GetIconResourceId();
  void SetIconResourceId(int resource_id);
  // The icon is tinted to default_icon_color_blue by default.
  // Call this method to display icons of original colors.
  void DisableIconTint();
  int GetSecondaryIconResourceId();
  void SetSecondaryIconResourceId(int resource_id);

  void SetSecondaryActionCallback(base::OnceClosure callback);

  void SetDuration(long customDuration);

  // Following methods forward calls from java to provided callbacks.
  void HandleActionClick(JNIEnv* env);
  void HandleSecondaryActionClick(JNIEnv* env);
  void HandleDismissCallback(JNIEnv* env, int dismiss_reason);

  const base::android::JavaRef<jobject>& GetJavaMessageWrapper() const;

  // Called by bridge when message is successfully enqueued.
  void SetMessageEnqueued();

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_message_wrapper_;
  base::OnceClosure action_callback_;
  base::OnceClosure secondary_action_callback_;
  DismissCallback dismiss_callback_;
  // True if message is in queue.
  bool message_enqueued_;
};

}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_MESSAGE_WRAPPER_H_
