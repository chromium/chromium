// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ASYNC_DOCUMENT_SUBRESOURCE_FILTER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ASYNC_DOCUMENT_SUBRESOURCE_FILTER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "components/subresource_filter/content/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

class MemoryMappedRuleset;

// Computes whether/how subresource filtering should be activated while loading
// |document_url| in a frame, based on the parent document's |activation_state|,
// the |parent_document_origin|, as well as any applicable deactivation rules in
// non-null |ruleset|.
mojom::ActivationState ComputeActivationState(
    const GURL& document_url,
    const url::Origin& parent_document_origin,
    const mojom::ActivationState& parent_activation_state,
    const MemoryMappedRuleset* ruleset);

// An asynchronous wrapper around DocumentSubresourceFilter (DSF).
//
// It is accessed on the UI thread and owns a DSF living on a dedicated
// sequenced |task_runner|. Provides asynchronous access to the DSF and destroys
// it asynchronously.
//
// Initially holds an empty filter in the synchronously created Core object, and
// initializes the filter on the |task_runner| asynchronously. This lets ADSF be
// created synchrously and be immediately used by clients on the UI thread,
// while the DSF is retrieved on the |task_runner| in a deferred manner.
class AsyncDocumentSubresourceFilter {
 public:
  using LoadPolicyCallback = base::OnceCallback<void(LoadPolicy)>;

  class Core;

  // Encapsulates the parameters needed for computing the frame-level
  // mojom::ActivationState appropriate for the frame this ADSF is created for.
  // These parameters are posted to ADSF::Core during its initialization.
  struct InitializationParams {
    InitializationParams();

    // Takes parameters needed for calculating the main-frame
    // mojom::ActivationState, that is: the main-frame |document| URL, the
    // page-level |activation_level|, and whether or not to
    // |measure_performance|.
    InitializationParams(GURL document_url,
                         mojom::ActivationLevel activation_level,
                         bool measure_performance);

    // Takes parameters needed for calculating the sub-frame
    // mojom::ActivationState, that is: the sub-frame |document| URL, the origin
    // of its |parent|, as well as the parent's |activation_state|.
    InitializationParams(GURL document_url,
                         url::Origin parent_document_origin,
                         mojom::ActivationState parent_activation_state);

    ~InitializationParams();

    InitializationParams(InitializationParams&& other);
    InitializationParams& operator=(InitializationParams&& other);

    // Parameters used to compute mojom::ActivationState for the |document|
    // before creating a DocumentSubresourceFilter.
    GURL document_url;
    url::Origin parent_document_origin;
    mojom::ActivationState parent_activation_state;

   private:
    DISALLOW_COPY_AND_ASSIGN(InitializationParams);
  };

  // Creates a Core and initializes it asynchronously on a |task_runner| using
  // the supplied initialization |params| and a VerifiedRuleset taken from the
  // |ruleset_handle|. The core remains owned by |this| object, but lives on
  // (and is accessed on) the |task_runner|.
  //
  // Once the mojom::ActivationState for the current frame is calculated, it is
  // reported back via |activation_state_callback| on the task runner associated
  // with the current thread. If MemoryMappedRuleset is not present or
  // malformed, then a default mojom::ActivationState is reported (with
  // mojom::ActivationLevel equal to DISABLED).
  //
  // If deleted before |activation_state_callback| is called, the callback will
  // never be called.
  AsyncDocumentSubresourceFilter(
      VerifiedRuleset::Handle* ruleset_handle,
      InitializationParams params,
      base::OnceCallback<void(mojom::ActivationState)>
          activation_state_callback);

  ~AsyncDocumentSubresourceFilter();

  // Computes LoadPolicy on a |task_runner| and returns it back to the calling
  // thread via |result_callback|. If MemoryMappedRuleset is not present or
  // malformed, then a LoadPolicy::Allow is returned.
  void GetLoadPolicyForSubdocument(const GURL& subdocument_url,
                                   LoadPolicyCallback result_callback);

  // Invokes |first_disallowed_load_callback|, if necessary, and posts a task to
  // call DocumentSubresourceFilter::reportDisallowedCallback() on the
  // |task_runner|.
  void ReportDisallowedLoad();

  // Should only be called for main frames. Updates |activation_state_| with the
  // more accurate |updated_page_state|, but retains ruleset specific properties
  // like document whitelisting. Must be called after initial activation state
  // is computed.
  //
  // Posts a task to update the state in |core_|, so any calls to
  // GetLoadPolicyForSubdocument that are called before this will get the old
  // state.
  void UpdateWithMoreAccurateState(
      const mojom::ActivationState& updated_page_state);

  // Must be called after activation state computation is finished.
  const mojom::ActivationState& activation_state() const;

  bool has_activation_state() const { return activation_state_.has_value(); }

  // The |first_disallowed_load_callback|, if it is non-null, is invoked on the
  // first ReportDisallowedLoad() call.
  void set_first_disallowed_load_callback(base::OnceClosure callback) {
    first_disallowed_load_callback_ = std::move(callback);
  }

 private:
  void OnActivateStateCalculated(
      base::OnceCallback<void(mojom::ActivationState)>
          activation_state_callback,
      mojom::ActivationState activation_state);

  // Note: Raw pointer, |core_| already holds a reference to |task_runner_|.
  base::SequencedTaskRunner* task_runner_;
  std::unique_ptr<Core, base::OnTaskRunnerDeleter> core_;
  base::OnceClosure first_disallowed_load_callback_;

  base::Optional<mojom::ActivationState> activation_state_;

  base::SequenceChecker sequence_checker_;

  base::WeakPtrFactory<AsyncDocumentSubresourceFilter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AsyncDocumentSubresourceFilter);
};

// Holds a DocumentSubresourceFilter that is created in a deferred manner in
// Initialize(), provided there is a valid ruleset to work with.
class AsyncDocumentSubresourceFilter::Core {
 public:
  Core();
  ~Core();

  // Can return nullptr even after initialization in case MemoryMappedRuleset
  // was not present, or was malformed during it.
  DocumentSubresourceFilter* filter() {
    DCHECK(sequence_checker_.CalledOnValidSequence());
    return filter_ ? &filter_.value() : nullptr;
  }

 private:
  friend class AsyncDocumentSubresourceFilter;

  // Updates the mojom::ActivationState in |filter_|.
  void SetActivationState(const mojom::ActivationState& state);

  // Computes mojom::ActivationState from |params| and initializes a DSF using
  // it. Returns the computed activation state.
  mojom::ActivationState Initialize(InitializationParams params,
                                    VerifiedRuleset* verified_ruleset);

  base::Optional<DocumentSubresourceFilter> filter_;
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ASYNC_DOCUMENT_SUBRESOURCE_FILTER_H_
