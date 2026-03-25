// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Listener callback for the clipboardCopyInActiveGoogleDoc event.
callback OnClipboardCopyInActiveGoogleDocListener = undefined (DOMString url);

interface OnClipboardCopyInActiveGoogleDocEvent : ExtensionEvent {
  static undefined addListener(
      OnClipboardCopyInActiveGoogleDocListener listener);
  static undefined removeListener(
      OnClipboardCopyInActiveGoogleDocListener listener);
  static boolean hasListener(OnClipboardCopyInActiveGoogleDocListener listener);
};

// API which provides support for ChromeOS Accessibility features in the
// browser.
[platforms=("chromeos"),
  implemented_in="chrome/browser/chromeos/extensions/accessibility_service_private/accessibility_service_private.h"]
interface AccessibilityServicePrivate {
  // Called when Select to Speak in ChromeOS should speak the current
  // text selection; fired when the context menu option was clicked in
  // a selection context.
  static Promise<undefined> speakSelectedText();

  // Called when Select to Speak in ChromeOS wants a clipboard copy
  // event to be performed on the active and focused tab with the
  // given URL. This is fired when Select to Speak is trying to speak
  // with search+s but cannot find a selection and the focused node
  // is in a Google Docs page.
  static attribute OnClipboardCopyInActiveGoogleDocEvent
      clipboardCopyInActiveGoogleDoc;
};

partial interface Browser {
  static attribute AccessibilityServicePrivate accessibilityServicePrivate;
};
