// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_NATIVE_WEB_KEYBOARD_EVENT_H_
#define COMPONENTS_INPUT_NATIVE_WEB_KEYBOARD_EVENT_H_

#include "base/time/time.h"
#include "build/build_config.h"
#include "base/component_export.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace ui {
class KeyEvent;
}

namespace input {

// Owns a platform specific event; used to pass own and pass event through
// platform independent code.
struct COMPONENT_EXPORT(INPUT) NativeWebKeyboardEvent :
    public blink::WebKeyboardEvent {
  NativeWebKeyboardEvent(blink::WebInputEvent::Type type,
                         int modifiers,
                         base::TimeTicks timestamp);

  // Creates a native web keyboard event from a WebKeyboardEvent. The |os_event|
  // member may be a synthetic event, and possibly incomplete.
  NativeWebKeyboardEvent(const blink::WebKeyboardEvent& web_event,
                         gfx::NativeView native_view);

  explicit NativeWebKeyboardEvent(gfx::NativeEvent native_event);
#if BUILDFLAG(IS_ANDROID)
  // Holds a global ref to android_key_event (allowed to be null).
  NativeWebKeyboardEvent(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& android_key_event,
      blink::WebInputEvent::Type type,
      int modifiers,
      base::TimeTicks timestamp,
      int keycode,
      int scancode,
      int unicode_character,
      bool is_system_key);
#else
  explicit NativeWebKeyboardEvent(const ui::KeyEvent& key_event);
#if defined(USE_AURA)
  // Create a legacy keypress event specified by |character|.
  NativeWebKeyboardEvent(const ui::KeyEvent& key_event, char16_t character);
#endif
#endif

  NativeWebKeyboardEvent(const NativeWebKeyboardEvent& event);
  ~NativeWebKeyboardEvent() override;

  NativeWebKeyboardEvent& operator=(const NativeWebKeyboardEvent& event);

  gfx::NativeEvent os_event;

  // True if this event should be ignored (e.g. in the browser) if it's not
  // handled by the renderer. This happens for RawKeyDown events that are
  // created while IME is active and is necessary to prevent backspace from
  // doing "history back" if it is hit in ime mode. Currently, it's only used by
  // Linux and Mac ports.
  bool skip_if_unhandled;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_NATIVE_WEB_KEYBOARD_EVENT_H_
