// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TEST_TOOLBAR_MODEL_H_
#define COMPONENTS_OMNIBOX_BROWSER_TEST_TOOLBAR_MODEL_H_

#include <stddef.h>
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/omnibox/browser/toolbar_model.h"

namespace gfx {
struct VectorIcon;
}

// A ToolbarModel that is backed by instance variables, which are initialized
// with some basic values that can be changed with the provided setters. This
// should be used only for testing.
class TestToolbarModel : public ToolbarModel {
 public:
  TestToolbarModel();
  ~TestToolbarModel() override;
  base::string16 GetFormattedFullURL() const override;
  base::string16 GetURLForDisplay() const override;
  GURL GetURL() const override;
  security_state::SecurityLevel GetSecurityLevel(
      bool ignore_editing) const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  base::string16 GetSecureVerboseText() const override;
  base::string16 GetSecureAccessibilityText() const override;
  base::string16 GetEVCertName() const override;
  bool ShouldDisplayURL() const override;
  bool IsOfflinePage() const override;

  void set_formatted_full_url(const base::string16& url) {
    formatted_full_url_ = std::make_unique<base::string16>(url);
  }
  void set_url_for_display(const base::string16& url) {
    url_for_display_ = std::make_unique<base::string16>(url);
  }
  void set_url(const GURL& url) { url_ = url; }
  void set_security_level(security_state::SecurityLevel security_level) {
    security_level_ = security_level;
  }
  void set_icon(const gfx::VectorIcon& icon) { icon_ = &icon; }
  void set_ev_cert_name(const base::string16& ev_cert_name) {
    ev_cert_name_ = ev_cert_name;
  }
  void set_should_display_url(bool should_display_url) {
    should_display_url_ = should_display_url;
  }
  void set_offline_page(bool offline_page) { offline_page_ = offline_page; }

 private:
  // If either of these is not explicitly set, the test class will return
  // |url_.spec()| for the URL for display or fully formatted URL.
  std::unique_ptr<base::string16> formatted_full_url_;
  std::unique_ptr<base::string16> url_for_display_;

  GURL url_;
  security_state::SecurityLevel security_level_ = security_state::NONE;
  const gfx::VectorIcon* icon_ = nullptr;
  base::string16 ev_cert_name_;
  bool should_display_url_ = false;
  bool offline_page_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestToolbarModel);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TEST_TOOLBAR_MODEL_H_
