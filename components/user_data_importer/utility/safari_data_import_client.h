// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORT_CLIENT_H_
#define COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORT_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/import/import_results.h"

// Interface for clients (e.g., UIs) which use the SafariDataImporter to
// import user data from Safari.
class SafariDataImportClient {
 public:
  virtual ~SafariDataImportClient() = default;

  // Phase one: parsing data

  // Triggered when the import fails entirely, e.g., due to an invalid file.
  virtual void OnTotalFailure() = 0;

  // Invoked when the number of bookmarks in the input file has been determined.
  virtual void OnBookmarksReady(size_t count) = 0;

  // Invoked when the number of history items in the input file has been
  // determined. Unlike other data types, this is an estimate and not an exact
  // count. An input file may contain one history file per Safari profile; the
  // names of these profiles are passed in `profiles`.
  virtual void OnHistoryReady(size_t estimated_count,
                              std::vector<std::u16string> profiles) = 0;

  // Invoked when the number of passwords in the input file has been determined.
  // The results object provides detailed information about passwords with a
  // conflict (i.e., those where the user already has a
  // different saved password for the same username/URL); the Client must use
  // this information to resolve conflicts and continue the import flow.
  virtual void OnPasswordsReady(
      const password_manager::ImportResults& results) = 0;

  // Invoked when the number of payment cards in the input file has been
  // determined.
  virtual void OnPaymentCardsReady(size_t count) = 0;

  // Phase two: executing import

  // Invoked when importing of bookmarks has completed. `count` is the number
  // which were successfully imported.
  virtual void OnBookmarksImported(size_t count) = 0;

  // Invoked when importing of history has completed. `count` is the number of
  // entries which were successfully imported.
  virtual void OnHistoryImported(size_t count) = 0;

  // Invoked when importing of passwords has completed. The results object
  // includes detailed information about any errors that were encountered (such
  // as a password which did not have a valid URL), which can be used to surface
  // a UI with additional details.
  virtual void OnPasswordsImported(const password_manager::ImportResults&) = 0;

  // Invoked when importing of payment cards has completed. `count` is the
  // number which were successfully imported.
  virtual void OnPaymentCardsImported(size_t count) = 0;

  // Additional required behaviors

  // Implementers should hold their own WeakPtrFactory and implement this
  // method to vend weak pointers to `self`.
  virtual base::WeakPtr<SafariDataImportClient> AsWeakPtr() = 0;
};

#endif  // COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORT_CLIENT_H_
