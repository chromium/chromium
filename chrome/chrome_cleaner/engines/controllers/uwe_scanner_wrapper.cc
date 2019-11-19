// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/controllers/uwe_scanner_wrapper.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chrome/chrome_cleaner/chrome_utils/force_installed_extension.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"

namespace chrome_cleaner {

namespace {

// Calls any_of if the collection is not empty. Otherwise returns true.
template <class InputIterator, class UnaryPredicate>
bool EmptyOrAnyOf(InputIterator first,
                  InputIterator last,
                  UnaryPredicate pred) {
  if (first == last) {
    return true;
  }
  return std::any_of(first, last, pred);
}

// Determines if the |matcher| matches the |extension|.
// The |matcher| must also have |found_uws| in its |matcher.uws_id()| list.
// At least 1 of the values in each |matcher| fields must equal the value
// in the relevant |extension| field.
bool ExtensionMatchesMatcher(UwSId found_uws,
                             const UwEMatcher& matcher,
                             const ForceInstalledExtension& extension) {
  return std::any_of(
      matcher.uws_id().begin(), matcher.uws_id().end(),
      [&extension, &matcher, found_uws](uint32_t id) {
        if (id != found_uws) {
          return false;
        }
        UwEMatcher::MatcherCriteria criteria = matcher.criteria();
        auto extension_ids = criteria.extension_id();
        auto update_urls = criteria.update_url();
        auto install_method = criteria.install_method();
        return EmptyOrAnyOf(extension_ids.begin(), extension_ids.end(),
                            [&extension](std::string extension_id) {
                              return extension_id == extension.id.AsString();
                            }) &&
               EmptyOrAnyOf(update_urls.begin(), update_urls.end(),
                            [&extension](std::string update_url) {
                              return update_url == extension.update_url;
                            }) &&
               EmptyOrAnyOf(install_method.begin(), install_method.end(),
                            [&extension](int install_method) {
                              return install_method == extension.install_method;
                            });
      });
}

}  // namespace

UwEScannerWrapper::UwEScannerWrapper(
    std::unique_ptr<Scanner> scanner,
    UwEMatchers* matchers,
    const std::vector<ForceInstalledExtension>& force_installed_extensions)
    : scanner_(std::move(scanner)),
      matchers_(matchers),
      force_installed_extensions_(std::move(force_installed_extensions)) {
  DCHECK(matchers_);
  DCHECK(scanner_);
}

UwEScannerWrapper::UwEScannerWrapper(UwEScannerWrapper&& wrapper) = default;

UwEScannerWrapper& UwEScannerWrapper::operator=(UwEScannerWrapper&& other) =
    default;

UwEScannerWrapper::~UwEScannerWrapper() {
  DCHECK(IsCompletelyDone());
}

bool UwEScannerWrapper::Start(const FoundUwSCallback& found_uws_callback,
                              DoneCallback done_callback) {
  DCHECK(IsCompletelyDone());
  DCHECK(found_uws_callback);
  DCHECK(done_callback);

  found_uws_callback_ = found_uws_callback;

  return scanner_->Start(
      base::BindRepeating(&UwEScannerWrapper::FindUwE, base::Unretained(this)),
      std::move(done_callback));
}

void UwEScannerWrapper::Stop() {
  scanner_->Stop();
}

bool UwEScannerWrapper::IsCompletelyDone() const {
  return scanner_->IsCompletelyDone();
}

void UwEScannerWrapper::FindUwE(UwSId found_uws) {
  base::ScopedClosureRunner runner(
      base::BindOnce(found_uws_callback_, found_uws));
  if (force_installed_extensions_.empty()) {
    return;
  }
  if (!PUPData::IsKnownPUP(found_uws)) {
    return;
  }
  PUPData::PUP* pup = PUPData::GetPUP(found_uws);
  for (const ForceInstalledExtension& extension : force_installed_extensions_) {
    // TODO(b/116852553): This check is inefficient and will not scale if we add
    // many matchers.
    bool matches_any = std::any_of(
        matchers_->uwe_matcher().begin(), matchers_->uwe_matcher().end(),
        [&extension, found_uws](UwEMatcher matcher) {
          return ExtensionMatchesMatcher(found_uws, matcher, extension);
        });
    if (matches_any) {
      pup->matched_extensions.push_back(extension);
    }
  }
}

}  // namespace chrome_cleaner
