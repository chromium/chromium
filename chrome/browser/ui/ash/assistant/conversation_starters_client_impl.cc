// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/conversation_starters_client_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/assistant/conversation_starter.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/assistant/conversation_starters_parser.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

// Constants -------------------------------------------------------------------

// Endpoint.
constexpr char kBaseUrl[] =
    "https://assistant.google.com/proactivesuggestions/conversationstarters";

// Network.
constexpr int kMaxBodySizeBytes = 10 * 1024;
constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("conversation_starters_loader", R"(
          semantics: {
            sender: "Google Assistant Conversation Starters"
            description:
              "The Google Assistant requests conversation starters to suggest "
              "potential actions for the user at the start of a new UI session."
            trigger:
              "Start of a new Assistant UI session."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy: {
            cookies_allowed: YES
          })");

// Param Keys.
constexpr char kLocaleParamKey[] = "hl";

// Loader ----------------------------------------------------------------------

class Loader {
 public:
  explicit Loader(scoped_refptr<network::SharedURLLoaderFactory> factory)
      : factory_(factory) {}

  Loader(const Loader& copy) = delete;
  Loader& operator=(const Loader& assign) = delete;

  ~Loader() {
    is_destroying_ = true;

    if (callback_)
      std::move(callback_).Run(std::vector<ash::ConversationStarter>());
  }

  void Start(ConversationStartersClientImpl::Callback callback) {
    callback_ = std::move(callback);

    // Setup request.
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = net::AppendOrReplaceQueryParameter(
        GURL(kBaseUrl), kLocaleParamKey, base::i18n::GetConfiguredLocale());

    // Setup loader.
    loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                               kNetworkTrafficAnnotationTag);

    // Start loading.
    loader_->DownloadToString(
        factory_.get(),
        base::BindOnce(&Loader::OnComplete, weak_factory_.GetWeakPtr()),
        kMaxBodySizeBytes);
  }

  bool is_destroying() const { return is_destroying_; }

 private:
  void OnComplete(std::unique_ptr<std::string> safe_json_response) {
    if (!callback_)
      return;

    // Verify |safe_json_response| is OK.
    if (loader_->NetError() != net::OK || !safe_json_response) {
      LOG(ERROR) << net::ErrorToString(loader_->NetError());
      std::move(callback_).Run(std::vector<ash::ConversationStarter>());
      return;
    }

    // Return conversation starters parsed from |safe_json_response|.
    // Note that this collection may be empty in the event of a parse error.
    std::move(callback_).Run(
        ConversationStartersParser::Parse(*safe_json_response));
  }

  scoped_refptr<network::SharedURLLoaderFactory> const factory_;
  ConversationStartersClientImpl::Callback callback_;
  std::unique_ptr<network::SimpleURLLoader> loader_;

  bool is_destroying_ = false;

  base::WeakPtrFactory<Loader> weak_factory_{this};
};

}  // namespace

// ConversationStartersClientImpl ----------------------------------------------

ConversationStartersClientImpl::ConversationStartersClientImpl(Profile* profile)
    : profile_(profile) {}

ConversationStartersClientImpl::~ConversationStartersClientImpl() = default;

void ConversationStartersClientImpl::FetchConversationStarters(
    Callback callback) {
  // Initialize and start a loader to fetch conversation starters from the
  // server. The created pointer will be deleted on load completion.
  auto* loader = new Loader(profile_->GetURLLoaderFactory());
  loader->Start(base::BindOnce(
      [](Loader* loader, Callback callback,
         std::vector<ash::ConversationStarter>&& conversation_starters) {
        // Pass on |conversation_starters| to the original |callback|.
        std::move(callback).Run(std::move(conversation_starters));

        // We only delete |loader| if this code is *not* being run as part of
        // its destruction sequence.
        if (!loader->is_destroying())
          delete loader;
      },
      base::Unretained(loader), std::move(callback)));
}
