// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUGGESTIONS_WEBUI_SUGGESTIONS_SOURCE_H_
#define COMPONENTS_SUGGESTIONS_WEBUI_SUGGESTIONS_SOURCE_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "components/suggestions/suggestions_service.h"
#include "url/gurl.h"

namespace suggestions {

// SuggestionsSource renders a webpage to list SuggestionsService data.
class SuggestionsSource {
 public:
  SuggestionsSource(SuggestionsService* suggestions_service,
                    const std::string& base_url);
  ~SuggestionsSource();

  using GotDataCallback =
      base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>;

  void StartDataRequest(const std::string& path, GotDataCallback callback);
  std::string GetMimeType(const std::string& path) const;

 private:
  // Only used when servicing requests on the UI thread.
  SuggestionsService* suggestions_service_;

  // The base URL at which which the Suggestions WebUI lives in the context of
  // the embedder.
  const std::string base_url_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<SuggestionsSource> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SuggestionsSource);
};

}  // namespace suggestions

#endif  // COMPONENTS_SUGGESTIONS_WEBUI_SUGGESTIONS_SOURCE_H_
