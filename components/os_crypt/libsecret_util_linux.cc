// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/libsecret_util_linux.h"

#include <dlfcn.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

//
// LibsecretLoader
//

namespace {

// TODO(crbug.com/660005) A message that is attached to useless entries that we
// create, to explain its existence.
const char kExplanationMessage[] =
    "Because of quirks in the gnome libsecret API, Chrome needs to store a "
    "dummy entry to guarantee that this keyring was properly unlocked. More "
    "details at http://crbug.com/660005.";

}  // namespace

decltype(
    &::secret_password_store_sync) LibsecretLoader::secret_password_store_sync =
    nullptr;
decltype(
    &::secret_service_search_sync) LibsecretLoader::secret_service_search_sync =
    nullptr;
decltype(
    &::secret_password_clear_sync) LibsecretLoader::secret_password_clear_sync =
    nullptr;
decltype(&::secret_item_get_secret) LibsecretLoader::secret_item_get_secret =
    nullptr;
decltype(&::secret_item_get_created) LibsecretLoader::secret_item_get_created =
    nullptr;
decltype(
    &::secret_item_get_modified) LibsecretLoader::secret_item_get_modified =
    nullptr;
decltype(&::secret_value_get_text) LibsecretLoader::secret_value_get_text =
    nullptr;
decltype(
    &::secret_item_get_attributes) LibsecretLoader::secret_item_get_attributes =
    nullptr;
decltype(&::secret_item_load_secret_sync)
    LibsecretLoader::secret_item_load_secret_sync = nullptr;
decltype(&::secret_value_unref) LibsecretLoader::secret_value_unref = nullptr;

bool LibsecretLoader::libsecret_loaded_ = false;

const LibsecretLoader::FunctionInfo LibsecretLoader::kFunctions[] = {
    {"secret_item_get_secret",
     reinterpret_cast<void**>(&secret_item_get_secret)},
    {"secret_item_get_attributes",
     reinterpret_cast<void**>(&secret_item_get_attributes)},
    {"secret_item_get_created",
     reinterpret_cast<void**>(&secret_item_get_created)},
    {"secret_item_get_modified",
     reinterpret_cast<void**>(&secret_item_get_modified)},
    {"secret_item_load_secret_sync",
     reinterpret_cast<void**>(&secret_item_load_secret_sync)},
    {"secret_password_clear_sync",
     reinterpret_cast<void**>(&secret_password_clear_sync)},
    {"secret_password_store_sync",
     reinterpret_cast<void**>(&secret_password_store_sync)},
    {"secret_service_search_sync",
     reinterpret_cast<void**>(&secret_service_search_sync)},
    {"secret_value_get_text", reinterpret_cast<void**>(&secret_value_get_text)},
    {"secret_value_unref", reinterpret_cast<void**>(&secret_value_unref)},
};

LibsecretLoader::SearchHelper::SearchHelper() = default;
LibsecretLoader::SearchHelper::~SearchHelper() {
  if (error_)
    g_error_free(error_);
  if (results_)
    g_list_free_full(results_, &g_object_unref);
}

void LibsecretLoader::SearchHelper::Search(const SecretSchema* schema,
                                           GHashTable* attrs,
                                           int flags) {
  DCHECK(!results_);
  results_ = LibsecretLoader::secret_service_search_sync(
      nullptr,  // default secret service
      schema, attrs, static_cast<SecretSearchFlags>(flags),
      nullptr,  // no cancellable object
      &error_);
}

// static
bool LibsecretLoader::EnsureLibsecretLoaded() {
  return LoadLibsecret() && LibsecretIsAvailable();
}

// static
bool LibsecretLoader::LoadLibsecret() {
  if (libsecret_loaded_)
    return true;

  static void* handle = dlopen("libsecret-1.so.0", RTLD_NOW | RTLD_GLOBAL);
  if (!handle) {
    // We wanted to use libsecret, but we couldn't load it. Warn, because
    // either the user asked for this, or we autodetected it incorrectly. (Or
    // the system has broken libraries, which is also good to warn about.)
    // TODO(crbug.com/607435): Channel this message to the user-facing log
    VLOG(1) << "Could not load libsecret-1.so.0: " << dlerror();
    return false;
  }

  for (const auto& function : kFunctions) {
    dlerror();
    *function.pointer = dlsym(handle, function.name);
    const char* error = dlerror();
    if (error) {
      VLOG(1) << "Unable to load symbol " << function.name << ": " << error;
      dlclose(handle);
      return false;
    }
  }

  libsecret_loaded_ = true;
  // We leak the library handle. That's OK: this function is called only once.
  return true;
}

// static
bool LibsecretLoader::LibsecretIsAvailable() {
  if (!libsecret_loaded_)
    return false;
  // A dummy query is made to check for availability, because libsecret doesn't
  // have a dedicated availability function. For performance reasons, the query
  // is meant to return an empty result.
  LibsecretAttributesBuilder attrs;
  attrs.Append("application", "chrome-string_to_get_empty_result");
  const SecretSchema kDummySchema = {
      "_chrome_dummy_schema",
      SECRET_SCHEMA_DONT_MATCH_NAME,
      {{"application", SECRET_SCHEMA_ATTRIBUTE_STRING},
       {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING}}};

  SearchHelper helper;
  helper.Search(&kDummySchema, attrs.Get(),
                SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK);
  return helper.success();
}

// TODO(crbug.com/660005) This is needed to properly unlock the default keyring.
// We don't need to ever read it.
void LibsecretLoader::EnsureKeyringUnlocked() {
  const SecretSchema kDummySchema = {
      "_chrome_dummy_schema_for_unlocking",
      SECRET_SCHEMA_NONE,
      {{"explanation", SECRET_SCHEMA_ATTRIBUTE_STRING},
       {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING}}};

  GError* error = nullptr;
  bool success = LibsecretLoader::secret_password_store_sync(
      &kDummySchema, nullptr /* default keyring */,
      "Chrome Safe Storage Control" /* entry title */,
      "The meaning of life" /* password */, nullptr, &error, "explanation",
      kExplanationMessage,
      nullptr /* null-terminated variable argument list */);
  if (error) {
    VLOG(1) << "Dummy store to unlock the default keyring failed: "
            << error->message;
    g_error_free(error);
  } else if (!success) {
    VLOG(1) << "Dummy store to unlock the default keyring failed.";
  }
}

//
// LibsecretAttributesBuilder
//

LibsecretAttributesBuilder::LibsecretAttributesBuilder() {
  attrs_ = g_hash_table_new_full(g_str_hash, g_str_equal,
                                 nullptr,   // no deleter for keys
                                 nullptr);  // no deleter for values
}

LibsecretAttributesBuilder::~LibsecretAttributesBuilder() {
  g_hash_table_destroy(attrs_);
}

void LibsecretAttributesBuilder::Append(const std::string& name,
                                        const std::string& value) {
  name_values_.push_back(name);
  gpointer name_str =
      static_cast<gpointer>(const_cast<char*>(name_values_.back().c_str()));
  name_values_.push_back(value);
  gpointer value_str =
      static_cast<gpointer>(const_cast<char*>(name_values_.back().c_str()));
  g_hash_table_insert(attrs_, name_str, value_str);
}

void LibsecretAttributesBuilder::Append(const std::string& name,
                                        int64_t value) {
  Append(name, base::NumberToString(value));
}
