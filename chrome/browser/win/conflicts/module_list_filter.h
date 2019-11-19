// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_MODULE_LIST_FILTER_H_
#define CHROME_BROWSER_WIN_CONFLICTS_MODULE_LIST_FILTER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/win/conflicts/proto/module_list.pb.h"

struct ModuleInfoKey;
struct ModuleInfoData;

namespace base {
class FilePath;
}

// This class is used to determine if a module should be blacklisted or
// whitelisted depending on the ModuleList component (See module_list.proto).
class ModuleListFilter : public base::RefCountedThreadSafe<ModuleListFilter> {
 public:
  ModuleListFilter();

  bool Initialize(const base::FilePath& module_list_path);

  // Returns true if a module is whitelisted based on the hash of its basename
  // and code id.
  //
  // A whitelisted module should not trigger any warning to the user, nor
  // should it be blocked from loading into the process.
  //
  // Marked virtual to allow mocking.
  virtual bool IsWhitelisted(base::StringPiece module_basename_hash,
                             base::StringPiece module_code_id_hash) const;

  // Returns true if the module is whitelisted.
  //
  // This is a convenience wrapper for IsWhitelisted() that accepts a pair of
  // ModuleInfoKey and ModuleInfoData.
  bool IsWhitelisted(const ModuleInfoKey& module_key,
                     const ModuleInfoData& module_data) const;

  // Returns the BlacklistAction associated with a blacklisted module. Returns
  // null if the module is not blacklisted.
  //
  // A blacklisted module can cause instability if allowed into the process.
  //
  // The BlacklistAction indicates if the module should be allowed to load, and
  // which kind of message should be displayed to the user, if applicable.
  //
  // Marked virtual to allow mocking.
  virtual std::unique_ptr<chrome::conflicts::BlacklistAction> IsBlacklisted(
      const ModuleInfoKey& module_key,
      const ModuleInfoData& module_data) const;

 protected:
  virtual ~ModuleListFilter();

 private:
  friend class base::RefCountedThreadSafe<ModuleListFilter>;

  chrome::conflicts::ModuleList module_list_;

  // Indicates if Initalize() has been succesfully called.
  bool initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(ModuleListFilter);
};

#endif  // CHROME_BROWSER_WIN_CONFLICTS_MODULE_LIST_FILTER_H_
