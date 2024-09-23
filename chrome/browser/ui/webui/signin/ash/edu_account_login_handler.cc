// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/ash/edu_account_login_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace ash {

namespace {
constexpr char kImageFetcherUmaClientName[] =
    "EduAccountLoginProfileImageFetcher";
constexpr char kObfuscatedGaiaIdKey[] = "obfuscatedGaiaId";
constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation(
        "edu_account_login_profile_image_fetcher",
        R"(
        semantics {
          sender: "Profile image fetcher for EDU account login flow"
          description:
            "Retrieves profile images for user's parent accounts in EDU account"
            "login flow."
          trigger: "Triggered when child user opens account addition flow."
          data: "Account picture URL of GAIA accounts."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification: "Not implemented."
        })");
constexpr char kFetchAccessTokenResultHistogram[] =
    "AccountManager.EduCoexistence.FetchAccessTokenResult";
}  // namespace

EduAccountLoginHandler::EduAccountLoginHandler(
    const base::RepeatingClosure& close_dialog_closure)
    : close_dialog_closure_(close_dialog_closure) {
  network_state_informer_ = base::MakeRefCounted<NetworkStateInformer>();
  network_state_informer_->Init();
}

EduAccountLoginHandler::~EduAccountLoginHandler() {
  close_dialog_closure_.Run();
}

EduAccountLoginHandler::ProfileImageFetcher::ProfileImageFetcher(
    image_fetcher::ImageFetcher* image_fetcher,
    const std::map<std::string, GURL>& profile_image_urls,
    base::OnceCallback<void(std::map<std::string, gfx::Image> profile_images)>
        callback)
    : image_fetcher_(image_fetcher),
      profile_image_urls_(profile_image_urls),
      callback_(std::move(callback)) {}

EduAccountLoginHandler::ProfileImageFetcher::~ProfileImageFetcher() = default;

void EduAccountLoginHandler::ProfileImageFetcher::FetchProfileImages() {
  for (const auto& profile_image_url : profile_image_urls_) {
    FetchProfileImage(profile_image_url.first, profile_image_url.second);
  }
}

void EduAccountLoginHandler::ProfileImageFetcher::FetchProfileImage(
    const std::string& obfuscated_gaia_id,
    const GURL& profile_image_url) {
  if (!profile_image_url.is_valid()) {
    OnImageFetched(obfuscated_gaia_id, gfx::Image(),
                   image_fetcher::RequestMetadata());
    return;
  }

  image_fetcher::ImageFetcherParams params(traffic_annotation,
                                           kImageFetcherUmaClientName);
  GURL image_url_with_size(signin::GetAvatarImageURLWithOptions(
      profile_image_url, signin::kAccountInfoImageSize,
      true /* no_silhouette */));
  image_fetcher_->FetchImage(
      image_url_with_size,
      base::BindOnce(
          &EduAccountLoginHandler::ProfileImageFetcher::OnImageFetched,
          weak_ptr_factory_.GetWeakPtr(), obfuscated_gaia_id),
      std::move(params));
}

void EduAccountLoginHandler::ProfileImageFetcher::OnImageFetched(
    const std::string& obfuscated_gaia_id,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  fetched_profile_images_[obfuscated_gaia_id] = std::move(image);
  if (fetched_profile_images_.size() == profile_image_urls_.size()) {
    std::move(callback_).Run(std::move(fetched_profile_images_));
  }
}

void EduAccountLoginHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "isNetworkReady",
      base::BindRepeating(&EduAccountLoginHandler::HandleIsNetworkReady,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getParents",
      base::BindRepeating(&EduAccountLoginHandler::HandleGetParents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "parentSignin",
      base::BindRepeating(&EduAccountLoginHandler::HandleParentSignin,
                          base::Unretained(this)));
}

void EduAccountLoginHandler::OnJavascriptDisallowed() {
  list_family_members_fetcher_.reset();
  access_token_fetcher_.reset();
  gaia_auth_fetcher_.reset();
  profile_image_fetcher_.reset();
  get_parents_callback_id_.clear();
  parent_signin_callback_id_.clear();
}

void EduAccountLoginHandler::HandleIsNetworkReady(
    const base::Value::List& args) {
  AllowJavascript();

  bool is_network_ready =
      network_state_informer_->state() == NetworkStateInformer::ONLINE;
  ResolveJavascriptCallback(args[0], base::Value(is_network_ready));
}

void EduAccountLoginHandler::HandleGetParents(const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(args.size(), 1u);

  if (!get_parents_callback_id_.empty()) {
    // HandleGetParents call is already in progress, reject the callback.
    RejectJavascriptCallback(args[0], base::Value());
    return;
  }
  get_parents_callback_id_ = args[0].GetString();

  FetchFamilyMembers();
}

void EduAccountLoginHandler::HandleParentSignin(const base::Value::List& args) {
  CHECK_EQ(args.size(), 3u);
  CHECK(args[0].is_string());

  if (!parent_signin_callback_id_.empty()) {
    // HandleParentSignin call is already in progress, reject the callback.
    RejectJavascriptCallback(args[0], base::Value());
    return;
  }
  parent_signin_callback_id_ = args[0].GetString();

  const base::Value::Dict& parent = args[1].GetDict();
  const std::string* obfuscated_gaia_id =
      parent.FindString(kObfuscatedGaiaIdKey);
  DCHECK(obfuscated_gaia_id);

  const std::string* password = args[2].GetIfString();
  FetchAccessToken(*obfuscated_gaia_id, password ? *password : std::string());
}

void EduAccountLoginHandler::FetchFamilyMembers() {
  DCHECK(!list_family_members_fetcher_);
  Profile* profile = Profile::FromWebUI(web_ui());

  list_family_members_fetcher_ = FetchListFamilyMembers(
      *IdentityManagerFactory::GetForProfile(profile),
      profile->GetURLLoaderFactory(),
      base::BindOnce(
          &EduAccountLoginHandler::OnListFamilyMembersResponse,
          base::Unretained(this)));  // Unretained(.) is safe because `this`
                                     // owns `list_family_members_fetcher_`.
}

void EduAccountLoginHandler::FetchParentImages(
    base::Value::List parents,
    std::map<std::string, GURL> profile_image_urls) {
  DCHECK(!profile_image_fetcher_);
  image_fetcher::ImageFetcher* fetcher =
      ImageFetcherServiceFactory::GetForKey(
          Profile::FromWebUI(web_ui())->GetProfileKey())
          ->GetImageFetcher(image_fetcher::ImageFetcherConfig::kNetworkOnly);
  profile_image_fetcher_ = std::make_unique<ProfileImageFetcher>(
      fetcher, profile_image_urls,
      base::BindOnce(&EduAccountLoginHandler::OnParentProfileImagesFetched,
                     base::Unretained(this), std::move(parents)));
  profile_image_fetcher_->FetchProfileImages();
}

void EduAccountLoginHandler::FetchAccessToken(
    const std::string& obfuscated_gaia_id,
    const std::string& password) {
  DCHECK(!access_token_fetcher_);
  Profile* profile = Profile::FromWebUI(web_ui());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(GaiaConstants::kAccountsReauthOAuth2Scope);
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "EduAccountLoginHandler", identity_manager, scopes,
          base::BindOnce(
              &EduAccountLoginHandler::CreateReAuthProofTokenForParent,
              base::Unretained(this), std::move(obfuscated_gaia_id),
              std::move(password)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          signin::ConsentLevel::kSignin);
}

void EduAccountLoginHandler::FetchReAuthProofTokenForParent(
    const std::string& child_oauth_access_token,
    const std::string& parent_obfuscated_gaia_id,
    const std::string& parent_credential) {
  DCHECK(!gaia_auth_fetcher_);
  Profile* profile = Profile::FromWebUI(web_ui());

  gaia_auth_fetcher_ = std::make_unique<GaiaAuthFetcher>(
      this, gaia::GaiaSource::kChrome, profile->GetURLLoaderFactory());
  gaia_auth_fetcher_->StartCreateReAuthProofTokenForParent(
      child_oauth_access_token, parent_obfuscated_gaia_id, parent_credential);
}

void EduAccountLoginHandler::OnListFamilyMembersResponse(
    const supervised_user::ProtoFetcherStatus& status,
    std::unique_ptr<kidsmanagement::ListMembersResponse> response) {
  if (!status.IsOk()) {
    OnListFamilyMembersFailure(status);
    return;
  }
  OnListFamilyMembersSuccess(*response);
  // Release response.
}

void EduAccountLoginHandler::OnListFamilyMembersSuccess(
    const kidsmanagement::ListMembersResponse& response) {
  list_family_members_fetcher_.reset();
  base::Value::List parents;
  std::map<std::string, GURL> profile_image_urls;

  for (const auto& member : response.members()) {
    if (member.role() != kidsmanagement::HEAD_OF_HOUSEHOLD &&
        member.role() != kidsmanagement::PARENT) {
      continue;
    }

    base::Value::Dict parent;
    parent.Set("email", member.profile().email());
    parent.Set("displayName", member.profile().display_name());
    parent.Set(kObfuscatedGaiaIdKey, member.user_id());

    parents.Append(std::move(parent));
    profile_image_urls[member.user_id()] =
        GURL(member.profile().profile_image_url());
  }

  FetchParentImages(std::move(parents), profile_image_urls);
}

void EduAccountLoginHandler::OnListFamilyMembersFailure(
    const supervised_user::ProtoFetcherStatus& status) {
  list_family_members_fetcher_.reset();
  RejectJavascriptCallback(base::Value(get_parents_callback_id_),
                           base::Value::List());
  get_parents_callback_id_.clear();
}

void EduAccountLoginHandler::OnParentProfileImagesFetched(
    base::Value::List parents,
    std::map<std::string, gfx::Image> profile_images) {
  profile_image_fetcher_.reset();

  for (auto& parent : parents) {
    const std::string* obfuscated_gaia_id =
        parent.GetDict().FindString(kObfuscatedGaiaIdKey);
    DCHECK(obfuscated_gaia_id);
    std::string profile_image;
    if (profile_images[*obfuscated_gaia_id].IsEmpty()) {
      gfx::ImageSkia default_icon =
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGIN_DEFAULT_USER);

      profile_image = webui::GetBitmapDataUrl(
          default_icon.GetRepresentation(1.0f).GetBitmap());
    } else {
      profile_image = webui::GetBitmapDataUrl(
          profile_images[*obfuscated_gaia_id].AsBitmap());
    }
    parent.GetDict().Set("profileImage", profile_image);
  }

  ResolveJavascriptCallback(base::Value(get_parents_callback_id_), parents);
  get_parents_callback_id_.clear();
}

void EduAccountLoginHandler::CreateReAuthProofTokenForParent(
    const std::string& parent_obfuscated_gaia_id,
    const std::string& parent_credential,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  base::UmaHistogramEnumeration(kFetchAccessTokenResultHistogram, error.state(),
                                GoogleServiceAuthError::NUM_STATES);
  access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR)
        << "Could not get access token to create ReAuthProofToken for parent"
        << error.ToString();
    base::Value::Dict result;
    result.Set("isWrongPassword", false);
    RejectJavascriptCallback(base::Value(parent_signin_callback_id_), result);
    parent_signin_callback_id_.clear();
    return;
  }

  FetchReAuthProofTokenForParent(access_token_info.token,
                                 parent_obfuscated_gaia_id, parent_credential);
}

void EduAccountLoginHandler::OnReAuthProofTokenSuccess(
    const std::string& reauth_proof_token) {
  gaia_auth_fetcher_.reset();
  ResolveJavascriptCallback(base::Value(parent_signin_callback_id_),
                            base::Value(reauth_proof_token));
  parent_signin_callback_id_.clear();
}

void EduAccountLoginHandler::OnReAuthProofTokenFailure(
    const GaiaAuthConsumer::ReAuthProofTokenStatus error) {
  LOG(ERROR) << "Failed to fetch ReAuthProofToken for the parent, error="
             << static_cast<int>(error);
  gaia_auth_fetcher_.reset();

  base::Value::Dict result;
  result.Set("isWrongPassword",
             error == GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);
  RejectJavascriptCallback(base::Value(parent_signin_callback_id_), result);
  parent_signin_callback_id_.clear();
}

}  // namespace ash
