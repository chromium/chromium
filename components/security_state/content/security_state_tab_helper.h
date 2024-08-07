// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_STATE_CONTENT_SECURITY_STATE_TAB_HELPER_H_
#define COMPONENTS_SECURITY_STATE_CONTENT_SECURITY_STATE_TAB_HELPER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/types/strong_alias.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/common/security/security_style.h"

namespace content {
class WebContents;
}  // namespace content

// Tab helper provides the page's security status. This fundamental
// implementation uses the WebContent's `VisibleSecurityState` to access the
// `security_state::SecurityLevel`. Embedders override this class to provide
// additional information, e.g. about policy-installed certificates.
class SecurityStateTabHelper
    : public content::WebContentsUserData<SecurityStateTabHelper> {
 public:
  using UsesEmbedderInformation = base::StrongAlias<class BoolTag, bool>;

  SecurityStateTabHelper(const SecurityStateTabHelper&) = delete;
  SecurityStateTabHelper& operator=(const SecurityStateTabHelper&) = delete;

  ~SecurityStateTabHelper() override;

  // See security_state::GetSecurityLevel.
  security_state::SecurityLevel GetSecurityLevel();
  virtual std::unique_ptr<security_state::VisibleSecurityState>
  GetVisibleSecurityState();

  // Used by tests to specify a callback to be called when
  // GetVisibleSecurityState() is called.
  void set_get_security_level_callback_for_tests_(base::OnceClosure closure) {
    get_security_level_callback_for_tests_ = std::move(closure);
  }

  UsesEmbedderInformation uses_embedder_information() const {
    return uses_embedder_information_;
  }

 protected:
  SecurityStateTabHelper(content::WebContents* web_contents,
                         UsesEmbedderInformation uses_embedder_information);

  virtual bool UsedPolicyInstalledCertificate() const;

 private:
  friend class content::WebContentsUserData<SecurityStateTabHelper>;

  explicit SecurityStateTabHelper(content::WebContents* web_contents);

  base::OnceClosure get_security_level_callback_for_tests_;
  const UsesEmbedderInformation uses_embedder_information_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // COMPONENTS_SECURITY_STATE_CONTENT_SECURITY_STATE_TAB_HELPER_H_
