// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/key_press_handler_manager.h"

#include <string>

#include "base/bind.h"
#include "base/time/time.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

const content::NativeWebKeyboardEvent
    kDummyEvent(blink::WebInputEvent::Type::kUndefined, 0, base::TimeTicks());

// Dummy keyboard event handler: ignores the event, but appends the given |name|
// to a logging |target|.
bool DummyHandler(const char* name,
                  std::string* target,
                  const content::NativeWebKeyboardEvent& /*event*/) {
  target->append(name);
  return false;
}

// A delegate which logs the handlers instead of adding and removing them.
class LoggingDelegate : public KeyPressHandlerManager::Delegate {
 public:
  explicit LoggingDelegate(std::string* target) : target_(target) {}

  // KeyPressHandlerManager::Delegate:
  void AddHandler(const content::RenderWidgetHost::KeyPressEventCallback&
                      handler) override {
    target_->append("A");
    handler.Run(kDummyEvent);
  }
  void RemoveHandler(const content::RenderWidgetHost::KeyPressEventCallback&
                         handler) override {
    target_->append("R");
    handler.Run(kDummyEvent);
  }

 private:
  std::string* const target_;
};

}  // namespace

class KeyPressHandlerManagerTest : public testing::Test {
 public:
  KeyPressHandlerManagerTest() : delegate_(&callback_trace_) {}

 protected:
  content::RenderWidgetHost::KeyPressEventCallback CallbackForName(
      const char* name) {
    return base::BindRepeating(DummyHandler, name, &callback_trace_);
  }

  // String encoding the events related to adding and removing callbacks. For
  // example, "A1R1A2R2" means that callback "1" was added, then removed, and
  // then callback "2" was added and removed.
  std::string callback_trace_;

  LoggingDelegate delegate_;
};

// Removing should remove the previously added callback.
TEST_F(KeyPressHandlerManagerTest, AddRemove) {
  KeyPressHandlerManager manager(&delegate_);
  manager.RegisterKeyPressHandler(CallbackForName("1"));
  manager.RemoveKeyPressHandler();
  EXPECT_EQ("A1R1", callback_trace_);
}

// Registering a new callback should remove the old one.
TEST_F(KeyPressHandlerManagerTest, AddReplace) {
  KeyPressHandlerManager manager(&delegate_);
  manager.RegisterKeyPressHandler(CallbackForName("1"));
  manager.RegisterKeyPressHandler(CallbackForName("2"));
  manager.RemoveKeyPressHandler();
  EXPECT_EQ("A1R1A2R2", callback_trace_);
}

// Without registering a callback first, none should be removed.
TEST_F(KeyPressHandlerManagerTest, NoRemove) {
  KeyPressHandlerManager manager(&delegate_);
  manager.RemoveKeyPressHandler();
  EXPECT_EQ(std::string(), callback_trace_);
}

// Adding one callback twice should be a no-op.
TEST_F(KeyPressHandlerManagerTest, AddTwice) {
  KeyPressHandlerManager manager(&delegate_);
  auto callback = CallbackForName("1");
  manager.RegisterKeyPressHandler(callback);
  manager.RegisterKeyPressHandler(callback);
  manager.RemoveKeyPressHandler();
  EXPECT_EQ("A1R1", callback_trace_);
}

// Removing one callback twice should be a no-op.
TEST_F(KeyPressHandlerManagerTest, RemoveTwice) {
  KeyPressHandlerManager manager(&delegate_);
  manager.RegisterKeyPressHandler(CallbackForName("1"));
  manager.RemoveKeyPressHandler();
  manager.RemoveKeyPressHandler();
  EXPECT_EQ("A1R1", callback_trace_);
}

}  // namespace autofill
