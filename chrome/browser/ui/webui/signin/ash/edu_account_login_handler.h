// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_EDU_ACCOUNT_LOGIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_EDU_ACCOUNT_LOGIN_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/supervised_user/core/browser/kids_management_api_fetcher.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher_status.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace ash {

// Handler for EDU account login flow.
class EduAccountLoginHandler : public content::WebUIMessageHandler,
                               public GaiaAuthConsumer {
 public:
  explicit EduAccountLoginHandler(
      const base::RepeatingClosure& close_dialog_closure);
  ~EduAccountLoginHandler() override;
  EduAccountLoginHandler(const EduAccountLoginHandler&) = delete;
  EduAccountLoginHandler& operator=(const EduAccountLoginHandler&) = delete;

 private:
  class ProfileImageFetcher {
   public:
    // Create a new instance to fetch a set of images.
    // |profile_image_urls| is a map from obfuscated Gaia id to profile image
    // url. After all images are fetched, |callback| will be resolved with a
    // map from obfuscated Gaia id to gfx::Image. If image cannot be fetched
    // an empty gfx::Image() will be returned.
    ProfileImageFetcher(
        image_fetcher::ImageFetcher* image_fetcher,
        const std::map<std::string, GURL>& profile_image_urls,
        base::OnceCallback<
            void(std::map<std::string, gfx::Image> profile_images)> callback);
    ProfileImageFetcher(const ProfileImageFetcher&) = delete;
    ProfileImageFetcher& operator=(const ProfileImageFetcher&) = delete;
    ~ProfileImageFetcher();

    // Start fetching profile images.
    void FetchProfileImages();

   private:
    FRIEND_TEST_ALL_PREFIXES(EduAccountLoginHandlerTest,
                             ProfileImageFetcherTest);

    // Called for each profile provided in |profile_image_urls_|. If
    // |profile_image_url| is valid - fetches the image. Otherwise calls
    // |OnImageFetched| with an empty image.
    void FetchProfileImage(const std::string& obfuscated_gaia_id,
                           const GURL& profile_image_url);

    // Called for each profile provided in |profile_image_urls_|. After all
    // images are fetched resolves |callback_| with profile images.
    void OnImageFetched(const std::string& obfuscated_gaia_id,
                        const gfx::Image& image,
                        const image_fetcher::RequestMetadata& metadata);

    raw_ptr<image_fetcher::ImageFetcher> image_fetcher_ = nullptr;
    const std::map<std::string, GURL> profile_image_urls_;
    base::OnceCallback<void(std::map<std::string, gfx::Image> profile_images)>
        callback_;
    std::map<std::string, gfx::Image> fetched_profile_images_;
    base::WeakPtrFactory<ProfileImageFetcher> weak_ptr_factory_{this};
  };

  FRIEND_TEST_ALL_PREFIXES(EduAccountLoginHandlerTest, HandleGetParentsSuccess);
  FRIEND_TEST_ALL_PREFIXES(EduAccountLoginHandlerTest, HandleGetParentsFailure);
  FRIEND_TEST_ALL_PREFIXES(EduAccountLoginHandlerTest,
                           HandleParentSigninSuccess);
  FRIEND_TEST_ALL_PREFIXES(EduAccountLoginHandlerTest,
                           HandleParentSigninAccessTokenFailure);
  FRIEND_TEST_ALL_PREFIXES(EduAccountLoginHandlerTest,
                           HandleParentSigninReAuthProofTokenFailure);
  FRIEND_TEST_ALL_PREFIXES(EduAccountLoginHandlerTest, ProfileImageFetcherTest);
  FRIEND_TEST_ALL_PREFIXES(EduAccountLoginHandlerTest,
                           HandleIsNetworkReadyOnline);
  FRIEND_TEST_ALL_PREFIXES(EduAccountLoginHandlerTest,
                           HandleIsNetworkReadyOffline);

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

  void HandleIsNetworkReady(const base::Value::List& args);
  void HandleGetParents(const base::Value::List& args);
  void HandleParentSignin(const base::Value::List& args);

  virtual void FetchFamilyMembers();
  virtual void FetchParentImages(
      base::Value::List parents,
      std::map<std::string, GURL> profile_image_urls);
  virtual void FetchAccessToken(const std::string& obfuscated_gaia_id,
                                const std::string& password);

  virtual void FetchReAuthProofTokenForParent(
      const std::string& child_oauth_access_token,
      const std::string& parent_obfuscated_gaia_id,
      const std::string& parent_credential);

  // ListFamilyMembers fetch handlers.
  void OnListFamilyMembersResponse(
      const supervised_user::ProtoFetcherStatus& status,
      std::unique_ptr<kidsmanagement::ListMembersResponse> response);
  void OnListFamilyMembersSuccess(
      const kidsmanagement::ListMembersResponse& response);
  void OnListFamilyMembersFailure(
      const supervised_user::ProtoFetcherStatus& status);

  // ProfileImageFetcher callback
  void OnParentProfileImagesFetched(
      base::Value::List parents,
      std::map<std::string, gfx::Image> profile_images);

  // signin::PrimaryAccountAccessTokenFetcher callback
  void CreateReAuthProofTokenForParent(
      const std::string& parent_obfuscated_gaia_id,
      const std::string& parent_credential,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  // GaiaAuthConsumer overrides.
  void OnReAuthProofTokenSuccess(
      const std::string& reauth_proof_token) override;
  void OnReAuthProofTokenFailure(
      const GaiaAuthConsumer::ReAuthProofTokenStatus error) override;

  // Used for getting parent RAPT token.
  std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;

  // Used for getting child access token.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  base::RepeatingClosure close_dialog_closure_;
  // Reference to NetworkStateInformer that handles changes in network
  // state.
  scoped_refptr<NetworkStateInformer> network_state_informer_;
  std::unique_ptr<supervised_user::ListFamilyMembersFetcher>
      list_family_members_fetcher_;

  std::unique_ptr<ProfileImageFetcher> profile_image_fetcher_;
  std::string get_parents_callback_id_;
  std::string parent_signin_callback_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_EDU_ACCOUNT_LOGIN_HANDLER_H_
