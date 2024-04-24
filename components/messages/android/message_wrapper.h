// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_MESSAGE_WRAPPER_H_
#define COMPONENTS_MESSAGES_ANDROID_MESSAGE_WRAPPER_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "components/messages/android/message_enums.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace messages {

// |MessagesWrapper| represents a message for native feature code. It accepts
// callbacks for action and dismiss events and provides methods for setting
// message properties. After setting message's properties feature code can
// enqueue the message through |MessageDispatcherBridge|.
class MessageWrapper {
 public:
  using DismissCallback = base::OnceCallback<void(DismissReason)>;
  using SecondaryMenuItemSelectedCallback = base::RepeatingCallback<void(int)>;

  // ActionCallback and DismissCallback default to base::NullCallback.
  // Normally constructor with callbacks should be used, but this one is useful
  // in situations when dismiss callback needs to be set after MessageWrapper
  // creation because MessageWrapper instance needs to be bound to the callback.
  MessageWrapper(MessageIdentifier message_identifier);
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
  int GetPrimaryButtonTextMaxLines();
  void SetPrimaryButtonTextMaxLines(int max_lines);
  std::u16string GetSecondaryButtonMenuText();
  void SetSecondaryButtonMenuText(
      const std::u16string& secondary_button_menu_text);

  // Methods to manage secondary menu items.
  void SetSecondaryMenuMaxSize(SecondaryMenuMaxSize max_size);
  void AddSecondaryMenuItem(int item_id,
                            int resource_id,
                            const std::u16string& item_text);
  void AddSecondaryMenuItem(int item_id,
                            int resource_id,
                            const std::u16string& item_text,
                            const std::u16string& item_description);
  void ClearSecondaryMenuItems();
  void AddSecondaryMenuItemDivider();

  // When setting a message icon use ResourceMapper::MapToJavaDrawableId to
  // translate from chromium resource_id to Android drawable resource_id.
  int GetIconResourceId();
  void SetIconResourceId(int resource_id);
  bool IsValidIcon();
  void SetIcon(const SkBitmap& icon);
  void EnableLargeIcon(bool enabled);
  void SetIconRoundedCornerRadius(int radius);
  // The icon is tinted to default_icon_color_accent1 by default.
  // Call this method to display icons of original colors.
  void DisableIconTint();
  int GetSecondaryIconResourceId();
  void SetSecondaryIconResourceId(int resource_id);

  void SetSecondaryActionCallback(base::RepeatingClosure callback);
  void SetSecondaryMenuItemSelectedCallback(
      base::RepeatingCallback<void(int)> callback);

  void SetDuration(long customDuration);

  // Note that the message will immediately be dismissed after the primary
  // action callback is run. Making the message remain visible after the primary
  // action button is clicked is supported in the messages API in Java, but not
  // currently in here in the messages API in C++.
  void SetActionClick(base::OnceClosure callback);
  void SetDismissCallback(DismissCallback callback);

  // Following methods forward calls from java to provided callbacks.
  void HandleActionClick(JNIEnv* env);
  void HandleSecondaryActionClick(JNIEnv* env);
  void HandleSecondaryMenuItemSelected(JNIEnv* env, int item_id);
  void HandleDismissCallback(JNIEnv* env, int dismiss_reason);

  const base::android::JavaRef<jobject>& GetJavaMessageWrapper() const;

  // Called by the bridge when the message is successfully enqueued.
  // WindowAndroid reference is retained and used for locating MessageDispatcher
  // instance when the message is dismissed.
  void SetMessageEnqueued(
      const base::android::JavaRef<jobject>& java_window_android);

  const base::android::JavaRef<jobject>& java_window_android() {
    return java_window_android_;
  }

  const SkBitmap GetIconBitmap();

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_message_wrapper_;
  base::OnceClosure action_callback_;
  base::RepeatingClosure secondary_action_callback_;
  SecondaryMenuItemSelectedCallback secondary_menu_item_selected_callback_;
  DismissCallback dismiss_callback_;
  // True if message is in queue.
  bool message_enqueued_;
  base::android::ScopedJavaGlobalRef<jobject> java_window_android_;

  SecondaryMenuMaxSize secondary_menu_max_size_ = SecondaryMenuMaxSize::SMALL;
};

}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_MESSAGE_WRAPPER_H_
