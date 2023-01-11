// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_CONTEXTUAL_SEARCH_DELEGATE_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_CONTEXTUAL_SEARCH_DELEGATE_H_

#include <stddef.h>

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/contextual_search/core/browser/contextual_search_context.h"
#include "components/contextual_search/core/browser/resolved_search_term.h"

namespace content {
class WebContents;
}

class ContextualSearchDelegate {
 public:
  // Provides text surrounding the selection to Java.
  typedef base::RepeatingCallback<
      void(const std::string&, const std::u16string&, size_t, size_t)>
      SurroundingTextCallback;
  // Provides the Resolved Search Term, called when the Resolve Request returns.
  typedef base::RepeatingCallback<void(const ResolvedSearchTerm&)>
      SearchTermResolutionCallback;

  virtual ~ContextualSearchDelegate() = default;

  // Gathers surrounding text and saves it in the given context. The given
  // callback will be run when the surrounding text becomes available.
  virtual void GatherAndSaveSurroundingText(
      base::WeakPtr<ContextualSearchContext> contextual_search_context,
      content::WebContents* web_contents,
      SurroundingTextCallback callback) = 0;

  // Starts an asynchronous search term resolution request.
  // The given context may include some content from a web page and must be able
  // to resolve.
  // When the response is available the given callback will be run.
  virtual void StartSearchTermResolutionRequest(
      base::WeakPtr<ContextualSearchContext> contextual_search_context,
      content::WebContents* web_contents,
      SearchTermResolutionCallback callback) = 0;
};

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_CONTEXTUAL_SEARCH_DELEGATE_H_
