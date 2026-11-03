// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_BROWSER_SHORTCUT_MANAGER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_BROWSER_SHORTCUT_MANAGER_ANDROID_H_

#include <jni.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/accelerators/accelerator.h"

namespace ui {
class AcceleratorManagerAndroid;
}  // namespace ui

class BrowserCommandDelegateAndroid;

// Connects the Java and C++ components of the shortcut manager.
// Registers GetAcceleratorList() on the window's ui::AcceleratorManagerAndroid
// as an AcceleratorTarget and routes matched IDC_* to
// BrowserCommandDelegateAndroid. MUST be destroyed before the
// ui::AcceleratorManagerAndroid it targets, to avoid use-after-free when
// unregistering accelerators.
class BrowserShortcutManagerAndroid : public ui::AcceleratorTarget {
 public:
  BrowserShortcutManagerAndroid(
      ui::AcceleratorManagerAndroid* accelerator_manager,
      std::unique_ptr<BrowserCommandDelegateAndroid> command_delegate);

  BrowserShortcutManagerAndroid(const BrowserShortcutManagerAndroid&) = delete;
  BrowserShortcutManagerAndroid& operator=(
      const BrowserShortcutManagerAndroid&) = delete;

  ~BrowserShortcutManagerAndroid() override;

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // Called by JNI only.
  int32_t PreHandleKeyEvent(JNIEnv* env,
                            const base::android::JavaRef<jobject>& j_key_event);

  void Destroy(JNIEnv* env);

 private:
  const raw_ptr<ui::AcceleratorManagerAndroid> accelerator_manager_;
  std::unique_ptr<BrowserCommandDelegateAndroid> command_delegate_;
  std::map<ui::Accelerator, int> accelerator_table_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_BROWSER_SHORTCUT_MANAGER_ANDROID_H_
