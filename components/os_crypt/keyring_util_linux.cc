// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/keyring_util_linux.h"

#include <dlfcn.h>

#include "base/logging.h"

decltype(&::gnome_keyring_is_available)
    GnomeKeyringLoader::gnome_keyring_is_available_ptr;
decltype(&::gnome_keyring_store_password)
    GnomeKeyringLoader::gnome_keyring_store_password_ptr;
decltype(&::gnome_keyring_delete_password)
    GnomeKeyringLoader::gnome_keyring_delete_password_ptr;
decltype(&::gnome_keyring_find_items)
    GnomeKeyringLoader::gnome_keyring_find_items_ptr;
decltype(&::gnome_keyring_find_password_sync)
    GnomeKeyringLoader::gnome_keyring_find_password_sync_ptr;
decltype(&::gnome_keyring_store_password_sync)
    GnomeKeyringLoader::gnome_keyring_store_password_sync_ptr;

decltype(&::gnome_keyring_result_to_message)
    GnomeKeyringLoader::gnome_keyring_result_to_message_ptr;
decltype(&::gnome_keyring_attribute_list_free)
    GnomeKeyringLoader::gnome_keyring_attribute_list_free_ptr;
decltype(&::gnome_keyring_attribute_list_new)
    GnomeKeyringLoader::gnome_keyring_attribute_list_new_ptr;
decltype(&::gnome_keyring_attribute_list_append_string)
    GnomeKeyringLoader::gnome_keyring_attribute_list_append_string_ptr;
decltype(&::gnome_keyring_attribute_list_append_uint32)
    GnomeKeyringLoader::gnome_keyring_attribute_list_append_uint32_ptr;
decltype(&::gnome_keyring_free_password)
    GnomeKeyringLoader::gnome_keyring_free_password_ptr;

bool GnomeKeyringLoader::keyring_loaded = false;

const GnomeKeyringLoader::FunctionInfo GnomeKeyringLoader::functions[] = {
    {"gnome_keyring_is_available",
     reinterpret_cast<void**>(&gnome_keyring_is_available_ptr)},
    {"gnome_keyring_store_password",
     reinterpret_cast<void**>(&gnome_keyring_store_password_ptr)},
    {"gnome_keyring_delete_password",
     reinterpret_cast<void**>(&gnome_keyring_delete_password_ptr)},
    {"gnome_keyring_find_items",
     reinterpret_cast<void**>(&gnome_keyring_find_items_ptr)},
    {"gnome_keyring_find_password_sync",
     reinterpret_cast<void**>(&gnome_keyring_find_password_sync_ptr)},
    {"gnome_keyring_store_password_sync",
     reinterpret_cast<void**>(&gnome_keyring_store_password_sync_ptr)},

    {"gnome_keyring_result_to_message",
     reinterpret_cast<void**>(&gnome_keyring_result_to_message_ptr)},
    {"gnome_keyring_attribute_list_free",
     reinterpret_cast<void**>(&gnome_keyring_attribute_list_free_ptr)},
    {"gnome_keyring_attribute_list_new",
     reinterpret_cast<void**>(&gnome_keyring_attribute_list_new_ptr)},
    {"gnome_keyring_attribute_list_append_string",
     reinterpret_cast<void**>(&gnome_keyring_attribute_list_append_string_ptr)},
    {"gnome_keyring_attribute_list_append_uint32",
     reinterpret_cast<void**>(&gnome_keyring_attribute_list_append_uint32_ptr)},
    {"gnome_keyring_free_password",
     reinterpret_cast<void**>(&gnome_keyring_free_password_ptr)}};

/* Load the library and initialize the function pointers. */
bool GnomeKeyringLoader::LoadGnomeKeyring() {
  if (keyring_loaded)
    return true;

  void* handle = dlopen("libgnome-keyring.so.0", RTLD_NOW | RTLD_GLOBAL);
  if (!handle) {
    // We wanted to use GNOME Keyring, but we couldn't load it. Warn, because
    // either the user asked for this, or we autodetected it incorrectly. (Or
    // the system has broken libraries, which is also good to warn about.)
    LOG(WARNING) << "Could not load libgnome-keyring.so.0: " << dlerror();
    return false;
  }

  for (size_t i = 0; i < std::size(functions); ++i) {
    dlerror();
    *functions[i].pointer = dlsym(handle, functions[i].name);
    const char* error = dlerror();
    if (error) {
      LOG(ERROR) << "Unable to load symbol " << functions[i].name << ": "
                 << error;
      dlclose(handle);
      return false;
    }
  }

  keyring_loaded = true;
  // We leak the library handle. That's OK: this function is called only once.
  return true;
}
