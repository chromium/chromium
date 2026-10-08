// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_CORE_SAFETY_LIST_MANAGER_H_
#define COMPONENTS_ACTOR_CORE_SAFETY_LIST_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace actor {

class SafetyListManager {
 public:
  // LINT.IfChange(ParseResult)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ParseResult {
    // The Safety List was successfully parsed.
    kSuccess = 0,
    // The provided string was not valid JSON.
    kInvalidJson = 1,
    // The value associated with the key was not a list.
    kJsonKeyValueNotAList = 2,
    // A value in the list was not a dictionary.
    kJsonListValueNotADictionary = 3,
    // The `to` field was missing or not a string.
    kInvalidToField = 4,
    // The `to` field was not a valid URL pattern.
    kInvalidToUrlPattern = 5,
    // The `from` field was missing or not a string.
    kInvalidFromField = 6,
    // The `from` field was not a valid URL pattern.
    kInvalidFromUrlPattern = 7,
    // A value in the list was not in the expected format.
    kEntryFormatInvalid = 8,
    kMaxValue = kEntryFormatInvalid,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:SafetyListParseResult)

  // Verdicts that are supported by the safety lists.
  enum class Decision {
    // No decision was made by the safety lists.
    kNone,
    // The action is allowed by the safety lists.
    kAllow,
    // The action is blocked by the safety lists.
    kBlock,
  };

  using FindCallback = base::OnceCallback<void(Decision)>;
  using PaymentIframeOriginAllowedCallback = base::OnceCallback<void(bool)>;
  using LoadSafetyListsClosure =
      base::OnceCallback<std::optional<std::string>()>;

  ~SafetyListManager();

  SafetyListManager(const SafetyListManager&) = delete;
  SafetyListManager& operator=(const SafetyListManager&) = delete;
  SafetyListManager(SafetyListManager&&) = delete;
  SafetyListManager& operator=(SafetyListManager&&) = delete;

  static SafetyListManager* GetInstance();
  static std::unique_ptr<SafetyListManager> CreateForTesting();

  // Looks up the most specific rule applying to a navigation from `source` to
  // `destination`. If no such rule exists, invokes `callback` with
  // `Decision::kNone`.
  //
  // If the safety lists have not yet been parsed but the data is available for
  // parsing, this triggers parsing off the main thread before resolving the
  // decision. If the safety lists have not yet been parsed and the data is not
  // available for parsing (and no parse is currently in progress), this invokes
  // `callback` with `Decision::kNone`.
  void Find(const GURL& source, const GURL& destination, FindCallback callback);

  // Checks whether `origin` is in the payment iframe allowlist and invokes
  // `callback` with the result.
  //
  // If the safety lists have not yet been parsed but the data is available for
  // parsing, this triggers parsing off the main thread before resolving the
  // lookup. If the safety lists have not yet been parsed and the data is not
  // available for parsing (and no parse is currently in progress), this invokes
  // `callback` with `false`.
  void IsPaymentIframeOriginAllowed(
      const url::Origin& origin,
      PaymentIframeOriginAllowedCallback callback);

  // Stores `closure` to be invoked and parsed off the main thread when `Find()`
  // or `IsPaymentIframeOriginAllowed()` is next called.
  void SetLoadSafetyListsClosure(LoadSafetyListsClosure closure);

 private:
  // For singleton pattern.
  friend class base::NoDestructor<SafetyListManager>;
  SafetyListManager();

  using PendingFinds = std::vector<base::OnceClosure>;

  struct ParsedSafetyLists {
    std::unique_ptr<content_settings::HostIndexedContentSettings> settings;
    std::optional<base::flat_set<url::Origin>> payment_iframe_allowed_origins;
  };

  struct ParseResultsAndSettings {
    ParseResult allowed_result;
    ParseResult blocked_result;
    ParseResult payment_iframe_allowed_result;
    ParsedSafetyLists parsed_lists;
  };

  // Private static so it can use ParseResultsAndSettings.
  static ParseResultsAndSettings ParseSafetyListsInternal(
      std::string_view json_string);

  // Private static so it can use ParseSafetyListsInternal.
  static ParsedSafetyLists DoParseSafetyLists(LoadSafetyListsClosure closure);

  void OnParsedSafetyLists(ParsedSafetyLists parsed_lists);

  SEQUENCE_CHECKER(sequence_checker_);

  // Returns true iff there's a pending parse operation in progress.
  bool IsParseInProgress() const;

  // Fetches the underlying data and begins parsing it, if possible.
  void MaybeStartParse();

  // Synchronously evaluates the lookup against the current data.
  Decision FindSync(const GURL& source, const GURL& destination) const;
  bool IsPaymentIframeOriginAllowedSync(const url::Origin& origin) const;

  // Producer closure to read the raw JSON string when `Find()` is first called.
  // May be null. Must not be invoked on the main thread.
  LoadSafetyListsClosure load_safety_lists_closure_;

  // Calls to `Find` that have been deferred until parsing is complete.
  PendingFinds pending_finds_;

  // Settings for allowing/blocking navigations. Must not be nullptr.
  std::unique_ptr<content_settings::HostIndexedContentSettings>
      navigation_settings_ =
          std::make_unique<content_settings::HostIndexedContentSettings>();

  // Allowed origins for cross-origin payment iframes.
  base::flat_set<url::Origin> payment_iframe_allowed_origins_;

  // Used for `OnParsedSafetyLists` replies so in-flight parses can be
  // invalidated if a newer closure is provided before parsing finishes.
  base::WeakPtrFactory<SafetyListManager> parse_weak_ptr_factory_{this};

  base::WeakPtrFactory<SafetyListManager> weak_ptr_factory_{this};
};

void SetSafetyListsForTesting(SafetyListManager* manager, std::string json);

}  // namespace actor

#endif  // COMPONENTS_ACTOR_CORE_SAFETY_LIST_MANAGER_H_
