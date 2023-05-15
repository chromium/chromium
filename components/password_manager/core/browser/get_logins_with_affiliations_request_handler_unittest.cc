// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/get_logins_with_affiliations_request_handler.h"

#include "base/functional/callback.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"

#include "components/password_manager/core/browser/mock_password_store_consumer.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::VariantWith;

constexpr const char kTestWebURL1[] = "https://one.example.com/path";
constexpr const char kAffiliatedWebURL1[] = "https://noneexample.com/path";
constexpr const char kAffiliatedRealm1[] = "https://noneexample.com/";
constexpr const char kGroupWebURL1[] = "https://noneexample2.com/path";
constexpr const char kGroupRealm1[] = "https://noneexample2.com/";

const PasswordStoreBackendError kBackendError = PasswordStoreBackendError(
    PasswordStoreBackendErrorType::kUncategorized,
    PasswordStoreBackendErrorRecoveryType::kUnrecoverable);

PasswordFormDigest CreateHTMLFormDigest(base::StringPiece url_string) {
  return PasswordFormDigest{PasswordForm::Scheme::kHtml,
                            GURL(url_string).DeprecatedGetOriginAsURL().spec(),
                            GURL(url_string)};
}

// Creates a form.
std::unique_ptr<PasswordForm> CreateForm(base::StringPiece url_string,
                                         base::StringPiece16 username,
                                         base::StringPiece16 password) {
  std::unique_ptr<PasswordForm> form = std::make_unique<PasswordForm>();
  form->username_value = std::u16string(username);
  form->password_value = std::u16string(password);
  form->url = GURL(url_string);
  form->signon_realm = form->url.DeprecatedGetOriginAsURL().spec();
  return form;
}

std::vector<std::unique_ptr<PasswordForm>> MakeCopy(
    const std::vector<std::unique_ptr<PasswordForm>>& forms) {
  std::vector<std::unique_ptr<PasswordForm>> copy;
  for (const auto& form : forms)
    copy.push_back(std::make_unique<PasswordForm>(*form));
  return copy;
}

}  // namespace

class GetLoginsWithAffiliationsRequestHandlerTest : public testing::Test {
 public:
  GetLoginsWithAffiliationsRequestHandlerTest()
      : store_(new MockPasswordStoreInterface()),
        consumer_(new MockPasswordStoreConsumer()) {}

  MockPasswordStoreConsumer* consumer() { return consumer_.get(); }
  MockPasswordStoreInterface* store() { return store_.get(); }
  scoped_refptr<GetLoginsWithAffiliationsRequestHandler> MakeRequestHandler() {
    return base::MakeRefCounted<GetLoginsWithAffiliationsRequestHandler>(
        CreateHTMLFormDigest(kTestWebURL1), consumer_->GetWeakPtr(),
        store_.get());
  }

  void TearDown() override { store()->ShutdownOnUIThread(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<MockPasswordStoreInterface> store_;
  std::unique_ptr<MockPasswordStoreConsumer> consumer_;
};

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest, LoginsReceivedFirst) {
  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(CreateForm(kTestWebURL1, u"username1", u"password"));
  forms.push_back(CreateForm(kTestWebURL1, u"username2", u"password"));

  auto handler = MakeRequestHandler();
  handler->LoginsForFormClosure().Run(MakeCopy(forms));

  EXPECT_THAT(handler->AffiliationsClosure().Run({kAffiliatedRealm1}),
              ElementsAre(CreateHTMLFormDigest(kAffiliatedRealm1)));

  PasswordForm affiliated_form =
      *CreateForm(kAffiliatedWebURL1, u"username3", u"password");

  std::vector<std::unique_ptr<PasswordForm>> expected_forms = std::move(forms);
  expected_forms.push_back(std::make_unique<PasswordForm>(affiliated_form));
  expected_forms.back()->is_affiliation_based_match = true;

  EXPECT_CALL(*consumer(), OnGetPasswordStoreResultsOrErrorFrom(
                               _, LoginsResultsOrErrorAre(&expected_forms)));

  std::vector<std::unique_ptr<PasswordForm>> affiliated_forms;
  affiliated_forms.push_back(std::make_unique<PasswordForm>(affiliated_form));
  handler->NonFormLoginsClosure().Run(std::move(affiliated_forms));
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest,
       AffiliatedLoginsReceivedFirst) {
  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(CreateForm(kTestWebURL1, u"username1", u"password"));
  forms.push_back(CreateForm(kTestWebURL1, u"username2", u"password"));

  auto handler = MakeRequestHandler();
  EXPECT_THAT(handler->AffiliationsClosure().Run({kAffiliatedRealm1}),
              ElementsAre(CreateHTMLFormDigest(kAffiliatedRealm1)));
  std::unique_ptr<PasswordForm> affiliated_form =
      CreateForm(kAffiliatedWebURL1, u"username3", u"password");
  std::vector<std::unique_ptr<PasswordForm>> affiliated_forms;
  affiliated_forms.push_back(std::make_unique<PasswordForm>(*affiliated_form));
  handler->NonFormLoginsClosure().Run(std::move(affiliated_forms));

  std::vector<std::unique_ptr<PasswordForm>> expected_forms = MakeCopy(forms);

  expected_forms.push_back(std::make_unique<PasswordForm>(*affiliated_form));
  expected_forms.back()->is_affiliation_based_match = true;
  EXPECT_CALL(*consumer(), OnGetPasswordStoreResultsOrErrorFrom(
                               _, LoginsResultsOrErrorAre(&expected_forms)));

  handler->LoginsForFormClosure().Run(std::move(forms));
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest,
       GroupLoginsReceivedBeforeAffiliations) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFillingAcrossGroupedSites);
  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(CreateForm(kTestWebURL1, u"username1", u"password"));
  forms.push_back(CreateForm(kTestWebURL1, u"username2", u"password"));

  auto handler = MakeRequestHandler();
  handler->LoginsForFormClosure().Run(MakeCopy(forms));

  EXPECT_THAT(handler->GroupClosure().Run({kGroupRealm1}),
              ElementsAre(CreateHTMLFormDigest(kGroupRealm1)));
  std::vector<std::unique_ptr<PasswordForm>> group_forms;
  PasswordForm group_form =
      *CreateForm(kGroupWebURL1, u"username4", u"password");
  group_forms.push_back(std::make_unique<PasswordForm>(group_form));
  std::vector<std::unique_ptr<PasswordForm>> expected_forms = std::move(forms);
  expected_forms.push_back(std::make_unique<PasswordForm>(group_form));
  expected_forms.back()->is_affiliation_based_match = true;
  expected_forms.back()->is_grouped_match = true;
  handler->NonFormLoginsClosure().Run(std::move(group_forms));

  EXPECT_THAT(handler->AffiliationsClosure().Run({kAffiliatedRealm1}),
              ElementsAre(CreateHTMLFormDigest(kAffiliatedRealm1)));
  PasswordForm affiliated_form =
      *CreateForm(kAffiliatedWebURL1, u"username3", u"password");
  std::vector<std::unique_ptr<PasswordForm>> affiliated_forms;
  affiliated_forms.push_back(std::make_unique<PasswordForm>(affiliated_form));
  expected_forms.push_back(std::make_unique<PasswordForm>(affiliated_form));
  expected_forms.back()->is_affiliation_based_match = true;

  EXPECT_CALL(*consumer(), OnGetPasswordStoreResultsOrErrorFrom(
                               _, LoginsResultsOrErrorAre(&expected_forms)));
  handler->NonFormLoginsClosure().Run(std::move(affiliated_forms));
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest,
       GroupLoginsReceivedAfterAffiliations) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFillingAcrossGroupedSites);
  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(CreateForm(kTestWebURL1, u"username1", u"password"));
  forms.push_back(CreateForm(kTestWebURL1, u"username2", u"password"));

  auto handler = MakeRequestHandler();
  handler->LoginsForFormClosure().Run(MakeCopy(forms));

  EXPECT_THAT(handler->AffiliationsClosure().Run({kAffiliatedRealm1}),
              ElementsAre(CreateHTMLFormDigest(kAffiliatedRealm1)));
  PasswordForm affiliated_form =
      *CreateForm(kAffiliatedWebURL1, u"username3", u"password");
  std::vector<std::unique_ptr<PasswordForm>> affiliated_forms;
  affiliated_forms.push_back(std::make_unique<PasswordForm>(affiliated_form));
  handler->NonFormLoginsClosure().Run(std::move(affiliated_forms));
  std::vector<std::unique_ptr<PasswordForm>> expected_forms = std::move(forms);
  expected_forms.push_back(std::make_unique<PasswordForm>(affiliated_form));
  expected_forms.back()->is_affiliation_based_match = true;

  EXPECT_THAT(handler->GroupClosure().Run({kGroupRealm1}),
              ElementsAre(CreateHTMLFormDigest(kGroupRealm1)));
  std::vector<std::unique_ptr<PasswordForm>> group_forms;
  PasswordForm group_form =
      *CreateForm(kGroupWebURL1, u"username4", u"password");
  group_forms.push_back(std::make_unique<PasswordForm>(group_form));
  expected_forms.push_back(std::make_unique<PasswordForm>(group_form));
  expected_forms.back()->is_affiliation_based_match = true;
  expected_forms.back()->is_grouped_match = true;

  EXPECT_CALL(*consumer(), OnGetPasswordStoreResultsOrErrorFrom(
                               _, LoginsResultsOrErrorAre(&expected_forms)));

  handler->NonFormLoginsClosure().Run(std::move(group_forms));
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest, ConsumerNotNotified) {
  auto handler = MakeRequestHandler();
  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(CreateForm(kTestWebURL1, u"username1", u"password"));
  forms.push_back(CreateForm(kTestWebURL1, u"username2", u"password"));

  std::vector<std::unique_ptr<PasswordForm>> affiliated_forms;
  affiliated_forms.push_back(
      CreateForm(kAffiliatedWebURL1, u"username3", u"password"));

  consumer()->CancelAllRequests();
  EXPECT_CALL(*consumer(), OnGetPasswordStoreResultsOrErrorFrom).Times(0);

  handler->AffiliationsClosure().Run({kAffiliatedRealm1});
  handler->NonFormLoginsClosure().Run(std::move(affiliated_forms));
  handler->LoginsForFormClosure().Run(std::move(forms));
}

// Tests that handler lives out of scope it was declared.
TEST_F(GetLoginsWithAffiliationsRequestHandlerTest, LivesLongerThanScope) {
  base::OnceCallback<void(
      absl::variant<std::vector<std::unique_ptr<PasswordForm>>,
                    PasswordStoreBackendError>)>
      forms_callback;
  base::OnceCallback<std::vector<PasswordFormDigest>(
      const std::vector<std::string>&)>
      affiliations_callback;
  base::OnceCallback<void(
      absl::variant<std::vector<std::unique_ptr<PasswordForm>>,
                    PasswordStoreBackendError>)>
      affiliated_callback;

  {
    auto handler = MakeRequestHandler();
    forms_callback = handler->LoginsForFormClosure();
    affiliations_callback = handler->AffiliationsClosure();
    affiliated_callback = handler->LoginsForFormClosure();
  };

  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(CreateForm(kTestWebURL1, u"username1", u"password"));
  forms.push_back(CreateForm(kTestWebURL1, u"username2", u"password"));

  std::move(forms_callback).Run(MakeCopy(forms));

  EXPECT_CALL(*consumer(), OnGetPasswordStoreResultsOrErrorFrom(
                               _, LoginsResultsOrErrorAre(&forms)));
  std::move(affiliations_callback).Run({});
  std::move(affiliated_callback).Run({});
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest,
       LoginsErrorThenAffilationsSuccess) {
  auto handler = MakeRequestHandler();

  EXPECT_THAT(handler->AffiliationsClosure().Run({kAffiliatedRealm1}),
              ElementsAre(CreateHTMLFormDigest(kAffiliatedRealm1)));

  EXPECT_CALL(*consumer(),
              OnGetPasswordStoreResultsOrErrorFrom(
                  _, VariantWith<PasswordStoreBackendError>(kBackendError)));

  handler->LoginsForFormClosure().Run(kBackendError);

  PasswordForm affiliated_form =
      *CreateForm(kAffiliatedWebURL1, u"username3", u"password");
  std::vector<std::unique_ptr<PasswordForm>> affiliated_forms;
  affiliated_forms.push_back(std::make_unique<PasswordForm>(affiliated_form));
  handler->NonFormLoginsClosure().Run(std::move(affiliated_forms));
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest,
       AffiliatedSuccessThenLoginsError) {
  auto handler = MakeRequestHandler();

  EXPECT_THAT(handler->AffiliationsClosure().Run({kAffiliatedRealm1}),
              ElementsAre(CreateHTMLFormDigest(kAffiliatedRealm1)));

  EXPECT_CALL(*consumer(),
              OnGetPasswordStoreResultsOrErrorFrom(
                  _, VariantWith<PasswordStoreBackendError>(kBackendError)));

  PasswordForm affiliated_form =
      *CreateForm(kAffiliatedWebURL1, u"username3", u"password");
  std::vector<std::unique_ptr<PasswordForm>> affiliated_forms;
  affiliated_forms.push_back(std::make_unique<PasswordForm>(affiliated_form));
  handler->NonFormLoginsClosure().Run(std::move(affiliated_forms));
  handler->LoginsForFormClosure().Run(kBackendError);
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest,
       AffiliatedLoginsErrorThenLoginsSuccess) {
  auto handler = MakeRequestHandler();

  EXPECT_THAT(handler->AffiliationsClosure().Run({kAffiliatedRealm1}),
              ElementsAre(CreateHTMLFormDigest(kAffiliatedRealm1)));

  EXPECT_CALL(*consumer(),
              OnGetPasswordStoreResultsOrErrorFrom(
                  _, VariantWith<PasswordStoreBackendError>(kBackendError)));

  handler->NonFormLoginsClosure().Run(kBackendError);

  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(CreateForm(kTestWebURL1, u"username1", u"password"));
  forms.push_back(CreateForm(kTestWebURL1, u"username2", u"password"));
  handler->LoginsForFormClosure().Run(std::move(forms));
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest,
       LoginsSuccessThenAffiliatedLoginsError) {
  auto handler = MakeRequestHandler();

  EXPECT_THAT(handler->AffiliationsClosure().Run({kAffiliatedRealm1}),
              ElementsAre(CreateHTMLFormDigest(kAffiliatedRealm1)));

  EXPECT_CALL(*consumer(),
              OnGetPasswordStoreResultsOrErrorFrom(
                  _, VariantWith<PasswordStoreBackendError>(kBackendError)));
  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(CreateForm(kTestWebURL1, u"username1", u"password"));
  forms.push_back(CreateForm(kTestWebURL1, u"username2", u"password"));
  handler->LoginsForFormClosure().Run(std::move(forms));

  handler->NonFormLoginsClosure().Run(kBackendError);
}

}  // namespace password_manager
