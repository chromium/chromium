// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_MAIN_MENU_BUILDER_H_
#define CHROME_BROWSER_UI_COCOA_MAIN_MENU_BUILDER_H_

#import <Cocoa/Cocoa.h>

#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/optional.h"

namespace chrome {

// Creates the main menu bar using the name specified in |product_name|, and
// registers it as such on |nsapp|. The NSApplicationDelegate |app_delegate|
// is the target for specific, special menu items.
//
//
// Normally the main menu is built in a MainMenu.nib file, but NIB files files
// are hard to edit (especially cross-platform) and bring in a compile
// dependency on ibtool. Building the menu in code has a lower maintenance
// burden.
void BuildMainMenu(NSApplication* nsapp,
                   id<NSApplicationDelegate> app_delegate,
                   const base::string16& product_name,
                   bool is_pwa);

// Internal ////////////////////////////////////////////////////////////////////

namespace internal {

// Helper class that builds NSMenuItems from data. Instances of this class
// should not outlive an autorelease pool scope as it does not retain any
// Objective-C members.
//
// This builder follows a fluent-interface pattern where the setters are
// not prefixed with the typical "set_" and they return a reference to this
// for easier method chaining.
//
// This class is only exposed for testing.
class MenuItemBuilder {
 public:
  explicit MenuItemBuilder(int string_id = 0);

  MenuItemBuilder(const MenuItemBuilder&);
  MenuItemBuilder& operator=(const MenuItemBuilder&);

  ~MenuItemBuilder();

  // Converts the item to a separator. Only tag() is also applicable.
  MenuItemBuilder& is_separator() {
    DCHECK_EQ(string_id_, 0);
    is_separator_ = true;
    return *this;
  }

  MenuItemBuilder& target(id target) {
    target_ = target;
    return *this;
  }

  MenuItemBuilder& action(SEL action) {
    DCHECK(!action_);
    action_ = action;
    return *this;
  }

  MenuItemBuilder& tag(int tag) {
    tag_ = tag;
    return *this;
  }

  // Wires up the menu item to the CommandDispatcher based on an
  // IDC_ command code.
  MenuItemBuilder& command_id(int command_id) {
    return tag(command_id).action(@selector(commandDispatch:));
  }

  // Specifies the string to substitute for the $1 found in the string for
  // |string_id_|.
  MenuItemBuilder& string_format_1(const base::string16& arg) {
    string_arg1_ = arg;
    return *this;
  }

  MenuItemBuilder& submenu(std::vector<MenuItemBuilder> items) {
    submenu_ = std::move(items);
    return *this;
  }

  // Registers a custom key equivalent. Normally the key equivalent is looked
  // up via AcceleratorsCocoa based on the command_id(). If one is not present,
  // the one specified here is used instead.
  MenuItemBuilder& key_equivalent(NSString* key_equivalent,
                                  NSEventModifierFlags flags) {
    DCHECK((flags & NSEventModifierFlagShift) == 0)
        << "The shift modifier flag should be directly applied to the key "
           "equivalent.";
    key_equivalent_ = key_equivalent;
    key_equivalent_flags_ = flags;
    return *this;
  }

  // Marks the item as an alternate keyboard equivalent menu item.
  MenuItemBuilder& is_alternate() {
    is_alternate_ = true;
    return *this;
  }

  // Excludes this item from the menu if |condition| is true.
  MenuItemBuilder& remove_if(bool condition) {
    is_removed_ |= condition;
    return *this;
  }

  // Builds a NSMenuItem instance from the properties set on the Builder.
  base::scoped_nsobject<NSMenuItem> Build() const;

 private:
  bool is_separator_ = false;

  int string_id_ = 0;
  base::string16 string_arg1_;

  int tag_ = 0;

  id target_ = nil;
  SEL action_ = nil;

  NSString* key_equivalent_ = @"";
  NSEventModifierFlags key_equivalent_flags_ = 0;

  bool is_alternate_ = false;

  bool is_removed_ = false;

  base::Optional<std::vector<MenuItemBuilder>> submenu_;

  // Copy and assign allowed.
};

}  // namespace internal
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_COCOA_MAIN_MENU_BUILDER_H_
