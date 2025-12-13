// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_REDACTION_PARAMS_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_REDACTION_PARAMS_H_

#include "base/containers/flat_set.h"
#include "net/base/schemeful_site.h"
#include "url/origin.h"

namespace paint_preview {

// Holds parameters/configuration related to redaction of a screenshot
// captured by Paint Preview.
class RedactionParams {
 public:
  RedactionParams();

  RedactionParams(base::flat_set<url::Origin> allowed_origins,
                  base::flat_set<net::SchemefulSite> allowed_sites);

  RedactionParams(const RedactionParams&);
  RedactionParams& operator=(const RedactionParams&);
  RedactionParams(RedactionParams&&);
  RedactionParams& operator=(RedactionParams&&);

  ~RedactionParams();

  // Returns true if an iframe with the given origin should be redacted from the
  // screenshot; false otherwise.
  bool ShouldRedactSubframe(const url::Origin& frame_origin) const;

 private:
  // Internal struct to store redaction parameter state.
  struct State {
    State(base::flat_set<url::Origin> allowed_origins,
          base::flat_set<net::SchemefulSite> allowed_sites);

    State(const State&);
    State& operator=(const State&);
    State(State&&);
    State& operator=(State&&);

    ~State();

    // Whether the origin allowlist contains the given origin.
    bool AllowlistContainsOrigin(const url::Origin& origin) const;
    // Whether the site allowlist contains the given origin.
    bool AllowlistContainsSite(const url::Origin& origin) const;

    base::flat_set<url::Origin> allowed_origins;
    base::flat_set<net::SchemefulSite> allowed_sites;
  };

  // The internal state of the redaction params. If this is std::nullopt, no
  // redaction will occur.
  std::optional<State> state_ = std::nullopt;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_REDACTION_PARAMS_H_
