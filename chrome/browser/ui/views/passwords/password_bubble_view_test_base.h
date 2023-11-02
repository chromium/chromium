// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_BUBBLE_VIEW_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_BUBBLE_VIEW_TEST_BASE_H_

#include <memory>

#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/views/widget/widget.h"

// Base class for testing password bubble views, which can be constructed
// passing web_contents() and anchor_view(). Mock behaviors can be set for
// model_delegate_mock() and feature_manager_mock().
class PasswordBubbleViewTestBase : public ChromeViewsTestBase {
 public:
  PasswordBubbleViewTestBase();
  ~PasswordBubbleViewTestBase() override;

  // Should be called before showing the child bubble view.
  void CreateAnchorViewAndShow();

  TestingProfile* profile() { return profile_.get(); }
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }
  content::WebContents* web_contents() { return test_web_contents_.get(); }
  views::View* anchor_view() { return anchor_widget_->GetContentsView(); }
  PasswordsModelDelegateMock* model_delegate_mock() {
    return &model_delegate_mock_;
  }
  password_manager::MockPasswordFeatureManager* feature_manager_mock() {
    return &feature_manager_mock_;
  }

  void TearDown() override;

 private:
  content::RenderViewHostTestEnabler test_render_host_factories_;
  password_manager::StubPasswordManagerClient password_manager_client_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  testing::NiceMock<password_manager::MockPasswordFeatureManager>
      feature_manager_mock_;
  testing::NiceMock<PasswordsModelDelegateMock> model_delegate_mock_;
  base::WeakPtrFactory<PasswordsModelDelegate> model_delegate_weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_BUBBLE_VIEW_TEST_BASE_H_
