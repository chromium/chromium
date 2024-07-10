// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_SYNC_LIBSECRET_UTIL_LINUX_H_
#define COMPONENTS_OS_CRYPT_SYNC_LIBSECRET_UTIL_LINUX_H_

#include <libsecret/secret.h>

#include <list>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"

// Utility for dynamically loading libsecret.
class LibsecretLoader {
 public:
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::secret_item_get_attributes)
      secret_item_get_attributes;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::secret_item_get_created)
      secret_item_get_created;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::secret_item_get_modified)
      secret_item_get_modified;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::secret_item_get_secret)
      secret_item_get_secret;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::secret_item_load_secret_sync)
      secret_item_load_secret_sync;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::secret_password_clear_sync)
      secret_password_clear_sync;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::secret_password_store_sync)
      secret_password_store_sync;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::secret_service_search_sync)
      secret_service_search_sync;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::secret_value_get_text)
      secret_value_get_text;
  static COMPONENT_EXPORT(OS_CRYPT) decltype(&::secret_value_unref)
      secret_value_unref;

  // Wrapper for secret_service_search_sync that prevents common leaks. See
  // https://crbug.com/393395.
  class COMPONENT_EXPORT(OS_CRYPT) SearchHelper {
   public:
    SearchHelper();

    SearchHelper(const SearchHelper&) = delete;
    SearchHelper& operator=(const SearchHelper&) = delete;

    ~SearchHelper();

    // Search must be called exactly once for success() and results() to be
    // populated.
    void Search(const SecretSchema* schema, GHashTable* attrs, int flags);

    bool success() { return !error_; }

    GList* results() { return results_.get(); }
    GError* error() { return error_.get(); }

   private:
    struct GErrorDeleter {
      inline void operator()(GError* error) { g_error_free(error); }
    };

    struct GListDeleter {
      inline void operator()(GList* list) {
        g_list_free_full(list, &g_object_unref);
      }
    };

    // |results_| and |error_| are C-style objects owned by this instance.
    std::unique_ptr<GList, GListDeleter> results_ = nullptr;
    std::unique_ptr<GError, GErrorDeleter> error_ = nullptr;
  };

  LibsecretLoader() = delete;
  LibsecretLoader(const LibsecretLoader&) = delete;
  LibsecretLoader& operator=(const LibsecretLoader&) = delete;

  // Loads the libsecret library and checks that it responds to queries.
  // Returns false if either step fails.
  // Repeated calls check the responsiveness every time, but do not load the
  // the library again if already successful.
  static COMPONENT_EXPORT(OS_CRYPT) bool EnsureLibsecretLoaded();

  // Ensure that the default keyring is accessible. This won't prevent the user
  // from locking their keyring while Chrome is running.
  static COMPONENT_EXPORT(OS_CRYPT) void EnsureKeyringUnlocked();

 protected:
  static COMPONENT_EXPORT(OS_CRYPT) bool libsecret_loaded_;

 private:
  struct FunctionInfo {
    const char* name;
    // This field is not a raw_ptr<> because it was filtered by the rewriter
    // for: #global-scope
    RAW_PTR_EXCLUSION void** pointer;
  };

  static const FunctionInfo kFunctions[];

  // Load the libsecret binaries. Returns true on success.
  // If successful, the result is cached and the function can be safely called
  // multiple times.
  // Checking |LibsecretIsAvailable| is necessary after this to verify that the
  // service responds to queries.
  static bool LoadLibsecret();

  // True if the libsecret binaries have been loaded and the library responds
  // to queries.
  static bool LibsecretIsAvailable();
};

class COMPONENT_EXPORT(OS_CRYPT) LibsecretAttributesBuilder {
 public:
  LibsecretAttributesBuilder();

  LibsecretAttributesBuilder(const LibsecretAttributesBuilder&) = delete;
  LibsecretAttributesBuilder& operator=(const LibsecretAttributesBuilder&) =
      delete;

  ~LibsecretAttributesBuilder();

  void Append(const std::string& name, const std::string& value);

  void Append(const std::string& name, int64_t value);

  // GHashTable, its keys and values returned from Get() are destroyed in
  // |LibsecretAttributesBuilder| destructor.
  GHashTable* Get() { return attrs_; }

 private:
  // |name_values_| is a storage for strings referenced in |attrs_|.
  // TODO(crbug.com/41251661): Make implementation more robust by not depending
  // on the implementation details of containers. External objects keep
  // references to the objects stored in this container. Using a vector here
  // will fail the ASan tests, because it may move the objects and break the
  // references.
  std::list<std::string> name_values_;
  raw_ptr<GHashTable> attrs_;
};

#endif  // COMPONENTS_OS_CRYPT_SYNC_LIBSECRET_UTIL_LINUX_H_
