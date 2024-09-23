// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TEST_LOCATION_BAR_MODEL_H_
#define COMPONENTS_OMNIBOX_BROWSER_TEST_LOCATION_BAR_MODEL_H_

#include <stddef.h>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/location_bar_model.h"

namespace gfx {
struct VectorIcon;
}

// A LocationBarModel that is backed by instance variables, which are
// initialized with some basic values that can be changed with the provided
// setters. This should be used only for testing.
class TestLocationBarModel : public LocationBarModel {
 public:
  TestLocationBarModel();
  ~TestLocationBarModel() override;
  TestLocationBarModel(const TestLocationBarModel&) = delete;
  TestLocationBarModel& operator=(const TestLocationBarModel&) = delete;
  std::u16string GetFormattedFullURL() const override;
  std::u16string GetURLForDisplay() const override;
  GURL GetURL() const override;
  security_state::SecurityLevel GetSecurityLevel() const override;
  net::CertStatus GetCertStatus() const override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch = false) const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetSecureDisplayText() const override;
  std::u16string GetSecureAccessibilityText() const override;
  bool ShouldDisplayURL() const override;
  bool IsOfflinePage() const override;
  bool ShouldPreventElision() const override;
  bool ShouldUseUpdatedConnectionSecurityIndicators() const override;

  void set_formatted_full_url(const std::u16string& url) {
    formatted_full_url_ = std::make_unique<std::u16string>(url);
  }
  void set_url_for_display(const std::u16string& url) {
    url_for_display_ = std::make_unique<std::u16string>(url);
  }
  void set_url(const GURL& url) { url_ = url; }
  void set_security_level(security_state::SecurityLevel security_level) {
    security_level_ = security_level;
  }
  void set_cert_status(net::CertStatus cert_status) {
    cert_status_ = cert_status;
  }
  void set_icon(const gfx::VectorIcon& icon) { icon_ = &icon; }
  void set_should_display_url(bool should_display_url) {
    should_display_url_ = should_display_url;
  }
  void set_offline_page(bool offline_page) { offline_page_ = offline_page; }
  void set_secure_display_text(std::u16string secure_display_text) {
    secure_display_text_ = secure_display_text;
  }
  void set_should_prevent_elision(bool should_prevent_elision) {
    should_prevent_elision_ = should_prevent_elision;
  }

 private:
  // If either of these is not explicitly set, the test class will return
  // |url_.spec()| for the URL for display or fully formatted URL.
  std::unique_ptr<std::u16string> formatted_full_url_;
  std::unique_ptr<std::u16string> url_for_display_;

  GURL url_;
  security_state::SecurityLevel security_level_ = security_state::NONE;
  net::CertStatus cert_status_ = 0;
  raw_ptr<const gfx::VectorIcon> icon_ = nullptr;
  bool should_display_url_ = false;
  bool offline_page_ = false;
  std::u16string secure_display_text_ = std::u16string();
  bool should_prevent_elision_ = false;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TEST_LOCATION_BAR_MODEL_H_
