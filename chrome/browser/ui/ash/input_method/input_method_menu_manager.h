// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/observer_list.h"
#include "chrome/browser/ui/ash/input_method/input_method_menu_item.h"
#include "ui/chromeos/ui_chromeos_export.h"

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_INPUT_METHOD_MENU_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_INPUT_METHOD_MENU_MANAGER_H_

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
}  // namespace base

namespace ui {
namespace ime {

class UI_CHROMEOS_EXPORT InputMethodMenuManager {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the list of menu items is changed.
    virtual void InputMethodMenuItemChanged(
        InputMethodMenuManager* manager) = 0;
  };

  InputMethodMenuManager(const InputMethodMenuManager&) = delete;
  InputMethodMenuManager& operator=(const InputMethodMenuManager&) = delete;

  ~InputMethodMenuManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Obtains the singleton instance.
  static InputMethodMenuManager* GetInstance();

  // Sets the list of input method menu items. The list could be empty().
  void SetCurrentInputMethodMenuItemList(
      const InputMethodMenuItemList& menu_list);

  // Gets the list of input method menu items. The list could be empty().
  InputMethodMenuItemList GetCurrentInputMethodMenuItemList() const;

  // True if the key exists in the menu_list_.
  bool HasInputMethodMenuItemForKey(const std::string& key) const;

 private:
  InputMethodMenuManager();

  // For Singleton to be able to construct an instance.
  friend struct base::DefaultSingletonTraits<InputMethodMenuManager>;

  // Menu item list of the input method.  This is set by extension IMEs.
  InputMethodMenuItemList menu_list_;

  // Observers who will be notified when menu changes.
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_INPUT_METHOD_MENU_MANAGER_H_
