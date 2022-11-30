// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_KEYRING_UTIL_LINUX_H_
#define COMPONENTS_OS_CRYPT_KEYRING_UTIL_LINUX_H_

// libgnome-keyring has been deprecated in favor of libsecret.
// See: https://mail.gnome.org/archives/commits-list/2013-October/msg08876.html
//
// The define below turns off the deprecations, in order to avoid build
// failures with Gnome 3.12. When we move to libsecret, the define can be
// removed, together with the include below it.
//
// The porting is tracked in http://crbug.com/355223
#define GNOME_KEYRING_DEPRECATED
#define GNOME_KEYRING_DEPRECATED_FOR(x)
#include <gnome-keyring.h>

#include "base/component_export.h"

// Many of the gnome_keyring_* functions use variable arguments, which makes
// them difficult if not impossible to truly wrap in C. Therefore, we use
// appropriately-typed function pointers and scoping to make the fact that we
// might be dynamically loading the library almost invisible. As a bonus, we
// also get a simple way to mock the library for testing. Classes that inherit
// from GnomeKeyringLoader will use its versions of the gnome_keyring_*
// functions. Note that it has only static fields.
class GnomeKeyringLoader {
 public:
  static COMPONENT_EXPORT(OS_CRYPT) bool LoadGnomeKeyring();

  // Declare the actual function pointers that we'll use in client code.
  // These functions will contact the service.
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::gnome_keyring_is_available)
      gnome_keyring_is_available_ptr;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::gnome_keyring_store_password)
      gnome_keyring_store_password_ptr;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::gnome_keyring_delete_password)
      gnome_keyring_delete_password_ptr;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::gnome_keyring_find_items)
      gnome_keyring_find_items_ptr;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(
      &::gnome_keyring_find_password_sync) gnome_keyring_find_password_sync_ptr;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(
      &::gnome_keyring_store_password_sync)
      gnome_keyring_store_password_sync_ptr;

  // These functions do not contact the service.
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::gnome_keyring_result_to_message)
      gnome_keyring_result_to_message_ptr;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(
      &::gnome_keyring_attribute_list_free)
      gnome_keyring_attribute_list_free_ptr;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(
      &::gnome_keyring_attribute_list_new) gnome_keyring_attribute_list_new_ptr;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(
      &::gnome_keyring_attribute_list_append_string)
      gnome_keyring_attribute_list_append_string_ptr;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(
      &::gnome_keyring_attribute_list_append_uint32)
      gnome_keyring_attribute_list_append_uint32_ptr;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::gnome_keyring_free_password)
      gnome_keyring_free_password_ptr;
  // We also use gnome_keyring_attribute_list_index(), which is a macro and
  // can't be referenced.

 protected:
  // Set to true if LoadGnomeKeyring() has already succeeded.
  static COMPONENT_EXPORT(OS_CRYPT) bool keyring_loaded;

 private:
  struct FunctionInfo {
    const char* name;
    void** pointer;
  };

  // Make it easy to initialize the function pointers in LoadGnomeKeyring().
  static const FunctionInfo functions[];
};

#endif  // COMPONENTS_OS_CRYPT_KEYRING_UTIL_LINUX_H_
