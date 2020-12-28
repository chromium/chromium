// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_MESSAGE_WRAPPER_H_
#define COMPONENTS_MESSAGES_ANDROID_MESSAGE_WRAPPER_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/strings/string16.h"

namespace messages {

// |MessagesWrapper| represents a message for native feature code. It accepts
// callbacks for action and dismiss events and provides methods for setting
// message properties. After setting message's properties feature code can
// enqueue the message through |MessageDispatcherBridge|.
class MessageWrapper {
 public:
  MessageWrapper(base::OnceClosure action_callback,
                 base::OnceClosure dismiss_callback);
  ~MessageWrapper();

  MessageWrapper(const MessageWrapper&) = delete;
  MessageWrapper& operator=(const MessageWrapper&) = delete;

  // Methods to manipulate message properties. On Android the values are
  // propagated to Java and stored in |PropertyModel|.
  // TODO(pavely): Reevaluate if propagating values to Java immediately is
  // really necessary. Alternatively we could collect all the values in native
  // and pass them to Java when enqueueing the message.
  base::string16 GetTitle();
  void SetTitle(const base::string16& title);
  base::string16 GetDescription();
  void SetDescription(const base::string16& description);
  base::string16 GetPrimaryButtonText();
  void SetPrimaryButtonText(const base::string16& primary_button_text);
  base::string16 GetSecondaryActionText();
  void SetSecondaryActionText(const base::string16& secondary_action_text);

  // When setting a message icon use ResourceMapper::MapToJavaDrawableId to
  // translate from chromium resource_id to Android drawable resource_id.
  int GetIconResourceId();
  void SetIconResourceId(int resource_id);
  int GetSecondaryIconResourceId();
  void SetSecondaryIconResourceId(int resource_id);

  void SetSecondaryActionCallback(base::OnceClosure callback);

  // Following methods forward calls from java to provided callbacks.
  void HandleActionClick(JNIEnv* env);
  void HandleSecondaryActionClick(JNIEnv* env);
  void HandleDismissCallback(JNIEnv* env);

  const base::android::JavaRef<jobject>& GetJavaMessageWrapper() const;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_message_wrapper_;
  base::OnceClosure action_callback_;
  base::OnceClosure secondary_action_callback_;
  base::OnceClosure dismiss_callback_;
  bool message_dismissed_;
};

}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_MESSAGE_WRAPPER_H_