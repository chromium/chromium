// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_FACTORY_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"

class Browser;

namespace extensions {
class ExtensionMessageBubbleController;
}  // namespace extensions

// Create and show ExtensionMessageBubbles for either extensions that look
// suspicious and have therefore been disabled, or for extensions that are
// running in developer mode that we want to warn the user about.
class ExtensionMessageBubbleFactory {
 public:
  // An enum to allow us to override the default behavior for testing.
  enum OverrideForTesting {
    NO_OVERRIDE,
    OVERRIDE_ENABLED,
    OVERRIDE_DISABLED,
  };

  explicit ExtensionMessageBubbleFactory(Browser* browser);

  ExtensionMessageBubbleFactory(const ExtensionMessageBubbleFactory&) = delete;
  ExtensionMessageBubbleFactory& operator=(
      const ExtensionMessageBubbleFactory&) = delete;

  ~ExtensionMessageBubbleFactory();

  // Returns the controller for the bubble that should be shown, if any.
  std::unique_ptr<extensions::ExtensionMessageBubbleController> GetController();

  // Overrides the default behavior for testing.
  static void set_override_for_tests(OverrideForTesting override);

 private:
  const raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_FACTORY_H_
