// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/remote_model_execution_common.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"

namespace optimization_guide {

proto::ExecuteRequest CreateExecuteRequest(
    ModelBasedCapabilityKey feature,
    const google::protobuf::MessageLite& request_metadata) {
  proto::ExecuteRequest execute_request;
  execute_request.set_feature(ToModelExecutionFeatureProto(feature));
  *execute_request.mutable_request_metadata() = AnyWrapProto(request_metadata);
  return execute_request;
}

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotation(
    ModelBasedCapabilityKey feature) {
  switch (feature) {
    case ModelBasedCapabilityKey::kWallpaperSearch:
      return net::DefineNetworkTrafficAnnotation(
          "wallpaper_create_themes_model_execution",
          R"(
        semantics {
          sender: "Create themes with AI"
          description: "Create a wallpaper with AI for custom themes."
          trigger: "User opens a new tab and clicks Customize Chrome."
          destination: GOOGLE_OWNED_SERVICE
          data:
            "User selected characteristics of the theme such as subject, mood,"
            " visual style and color."
          internal {
            contacts {
              email: "chrome-intelligence-core@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: USER_CONTENT
          }
          last_reviewed: "2024-01-11"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this by signing-in to Chrome, and from Settings."
          chrome_policy {
            CreateThemesSettings {
              CreateThemesSettings: 2
            }
          }
        })");
    case ModelBasedCapabilityKey::kCompose:
      return net::DefineNetworkTrafficAnnotation(
          "help_me_write_model_execution", R"(
        semantics {
          sender: "Help me write"
          description:
            "Helps users to write content in a web form, such as for product "
            "reviews or emails."
          trigger: "User right-clicks on a text box and clicks Help me write."
          destination: GOOGLE_OWNED_SERVICE
          data:
            "User written input text, title, URL, and content of the page"
          internal {
            contacts {
              email: "chrome-intelligence-core@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: SENSITIVE_URL
            type: WEB_CONTENT
            type: USER_CONTENT
          }
          last_reviewed: "2024-01-11"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this by signing-in to Chrome, and from Settings."
          chrome_policy {
            HelpMeWriteSettings {
              HelpMeWriteSettings: 2
            }
          }
        })");
    case ModelBasedCapabilityKey::kPasswordChangeSubmission:
      return net::DefineNetworkTrafficAnnotation(
          "password_change_submission_model_execution", R"(
        semantics {
          sender: "Automated Password Change"
          description:
            "Analyze page content to find elements that open and submit"
            " password change forms for Chrome actuation."
            " Lastly identities if the password change was successful."
          trigger:
            "User logged-in with a compromised credential and accepted "
            "an option from the dialog to change password automatically."
          destination: GOOGLE_OWNED_SERVICE
          data:
            "Title, URL, and content of the page, which may "
            "potentially contain user input."
          internal {
            contacts {
              email: "chrome-intelligence-core@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: SENSITIVE_URL
            type: WEB_CONTENT
          }
          last_reviewed: "2026-07-03"
        }
        policy {
          cookies_allowed: NO
          setting:
            "There is no dedicated setting for this feature."
            "Users are free to choose whether to use the feature "
            "or not when it's offered."
          chrome_policy {
            AutomatedPasswordChangeSettings {
              AutomatedPasswordChangeSettings: 2
            }
          }
        })");
    case ModelBasedCapabilityKey::kTest:
    case ModelBasedCapabilityKey::kBlingPrototyping:
      // Used for testing purposes. No real features use this.
      return MISSING_TRAFFIC_ANNOTATION;
    case ModelBasedCapabilityKey::kFormsClassifications:
      return net::DefineNetworkTrafficAnnotation(
          "forms_classifications_model_execution", R"(
    semantics {
      sender: "AutofillAI - Forms Classifications"
      description:
        "Analyze page content to classify the types of form fields and store "
        "those classifications for subsequent autofilling of forms."
      trigger: "User encounters a web form on page load and the Autofill "
               "server has selected the form as relevant for model execution."
      destination: GOOGLE_OWNED_SERVICE
      data: "Title, URL, and content of the page."
      internal {
        contacts {
          email: "chrome-intelligence-core@google.com"
        }
      }
      user_data {
        type: ACCESS_TOKEN
        type: SENSITIVE_URL
        type: WEB_CONTENT
        type: USER_CONTENT
      }
      last_reviewed: "2025-04-23"
    }
    policy {
      cookies_allowed: NO
      setting:
        "Users can control this by signing-in to Chrome, and via the "
        "'Autofill with AI' setting in the 'Autofill and passwords' "
        "section."
      chrome_policy {
        AutofillPredictionSettings {
          AutofillPredictionSettings: 2
        }
      }
    })");
    case ModelBasedCapabilityKey::kEnhancedCalendar:
      // TODO(crbug.com/398296762): Add network traffic annotation.
      return MISSING_TRAFFIC_ANNOTATION;
    case ModelBasedCapabilityKey::kZeroStateSuggestions:
      return net::DefineNetworkTrafficAnnotation(
          "zero_state_suggestions_model_execution", R"(
    semantics {
      sender: "Gemini in Chrome - Zero State Suggestions"
      description:
        "Generates contextual suggestions about the current page when Gemini "
        "in Chrome does not have a query."
      trigger:
        "User opens Gemini in Chrome via browser entrypoint, OS entrypoint, or"
        " hot key."
      destination: GOOGLE_OWNED_SERVICE
      data:
        "Title, URL, and content of the page, which may potentially contain "
        "user input. The access token is also sent to verify user is of "
        "sufficient age to use Gemini in Chrome."
      internal {
        contacts {
          email: "chrome-intelligence-core@google.com"
        }
      }
      user_data {
        type: ACCESS_TOKEN
        type: SENSITIVE_URL
        type: WEB_CONTENT
      }
      last_reviewed: "2025-05-21"
    }
    policy {
      cookies_allowed: NO
      setting:
        "This feature can be disabled via GeminiSettings."
      chrome_policy {
        GeminiSettings {
          GeminiSettings: 1
        }
      }
    })");
    case ModelBasedCapabilityKey::kWalletablePassExtraction:
      // TODO(crbug.com/441680019): Add network traffic annotation.
      return MISSING_TRAFFIC_ANNOTATION;
    case ModelBasedCapabilityKey::kAmountExtraction:
      return net::DefineNetworkTrafficAnnotation(
          "amount_extraction_model_execution",
          R"(
    semantics {
      sender: "Amount Extraction"
      description:
        "Uses server-side AI model to extract the final checkout amount "
        "from a web page to support features like Buy Now Pay Later (BNPL). "
        "This helps improve the accuracy of amount extraction on checkout "
        "pages."
      trigger:
        "User navigates to a checkout page on a supported merchant website "
        "and selects bnpl option."
      destination: GOOGLE_OWNED_SERVICE
      data:
        "The text content of the checkout page, which may include the page "
        "URL, and user-input data in the checkout form."
      internal {
        contacts {
          email: "chrome-intelligence-core@google.com"
        }
      }
      user_data {
        type: WEB_CONTENT
        type: USER_CONTENT
      }
      last_reviewed: "2025-11-20"
    }
    policy {
      cookies_allowed: NO
      setting:
        "You can enable or disable this feature via the 'Pay over time' toggle "
        "in Chrome Settings > Autofill and Passwords > Payment methods. This "
        "feature is enabled by default. It also requires "
        "'Save and fill payment methods' to be enabled."
      chrome_policy {
        AutofillCreditCardEnabled {
          AutofillCreditCardEnabled: false
        }
      }
    })");
    case ModelBasedCapabilityKey::kIosSmartTabGrouping:
      // TODO(crbug.com/456457419): Add network traffic annotation.
      return MISSING_TRAFFIC_ANNOTATION;
    case ModelBasedCapabilityKey::kHistorySearch:
      // On-device only feature.
      NOTREACHED();
    case ModelBasedCapabilityKey::kScamDetection:
      return net::DefineNetworkTrafficAnnotation(
          "scam_detection_model_execution",
          R"(
    semantics {
      sender: "Safe Browsing Scam Detection"
      description:
        "Uses server-side AI model to extract the brand and intent of the page "
        "from a web page to determine if the page is scammy."
      trigger:
        "User navigates to a suspicious web page."
      destination: GOOGLE_OWNED_SERVICE
      data:
        "The text content of the suspicious page."
      internal {
        contacts {
          email: "xinghuilu@chromium.org"
        }
        contacts {
          email: "chrome-counter-abuse-alerts@google.com"
        }
      }
      user_data {
        type: WEB_CONTENT
      }
      last_reviewed: "2025-12-04"
    }
    policy {
      cookies_allowed: NO
      setting:
        "Users can enable this feature via the enhanced protection setting "
        "in Chrome Settings > Privacy and security > Security > Safe Browsing."
        "This feature is disabled by default."
      chrome_policy {
        SafeBrowsingProtectionLevel {
          SafeBrowsingProtectionLevel: 1
        }
      }
    })");
    case ModelBasedCapabilityKey::kSkills:
      // TODO(xinyuqian): Update the email address to the team email once the
      // feature is launched or ready for launch.
      return net::DefineNetworkTrafficAnnotation("skills_model_execution", R"(
      semantics {
        sender: "Skills in Chrome"
        description:
          "Skills are page-aware, reusable AI workflows built on top of Gemini "
          "in Chrome. They enable users to save, create, reuse, and discover "
          "high-value AI workflows directly in the browser."
        trigger:
          "The user interacts with a feature that refines a skill. "
        destination: GOOGLE_OWNED_SERVICE
        data:
          "The current page content or user-provided text relevant to skills."
        internal {
          contacts {
            email: "xinyuqian@google.com"
          }
        }
        user_data {
          type: SENSITIVE_URL
          type: WEB_CONTENT
          type: USER_CONTENT
        }
        last_reviewed: "2026-01-29"
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting:
          "This feature can be disabled by turning off the relevant AI "
          "feature in Chrome settings."
        chrome_policy {
          GeminiSettings {
            GeminiSettings: 1
          }
        }
      }
    )");
    case ModelBasedCapabilityKey::kGeminiAntiscamProtection:
      return net::DefineNetworkTrafficAnnotation(
          "gemini_antiscam_protection_model_execution",
          R"(
    semantics {
      sender: "Gemini Antiscam Protection"
      description:
        "Uses server-side AI model to learn more about the scamminess of a "
        "page."
      trigger:
        "User navigates to a suspicious web page and force request client side "
        "detection ping is triggered."
      destination: GOOGLE_OWNED_SERVICE
      data:
        "The URL and visible text content of the suspicious page the user is "
        "currently visiting."
      internal {
        contacts {
          email: "skrakowi@chromium.org"
        }
        contacts {
          email: "chrome-counter-abuse-alerts@google.com"
        }
      }
      user_data {
        type: SENSITIVE_URL
        type: WEB_CONTENT
        type: USER_CONTENT
      }
      last_reviewed: "2026-01-28"
    }
    policy {
      cookies_allowed: NO
      setting:
        "Users can enable this feature via the enhanced protection setting "
        "in Chrome Settings > Privacy and security > Security > Safe Browsing."
        "This feature is disabled by default."
      chrome_policy {
        SafeBrowsingProtectionLevel {
          SafeBrowsingProtectionLevel: 1
        }
      }
    })");
    case ModelBasedCapabilityKey::kContentAnnotation:
      // TODO(crbug.com/486232932): Add network traffic annotation.
      return MISSING_TRAFFIC_ANNOTATION;
    case ModelBasedCapabilityKey::kFinds:
      return net::DefineNetworkTrafficAnnotation("finds_model_execution", R"(
        semantics {
          sender: "Finds"
          description:
            "Suggests URLs based on user browsing history and interests."
          trigger:
            "The only way for end-users to access finds is by opting into the "
            "feature when prompted."
          destination: GOOGLE_OWNED_SERVICE
          data:
            "The user's browsing history over the past week, including URLs, "
            "page titles, and visit timestamps."
          internal {
            contacts {
              email: "wylieb@google.com"
            }
            contacts {
              email: "chrome-finds-team@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: SENSITIVE_URL
            type: WEB_CONTENT
            type: USER_CONTENT
          }
          last_reviewed: "2026-03-19"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is only available to users who explicitly opt-in. It"
            " can be disabled through android notification settings, and also "
            "controlled by the GenAI policy and the Finds policy."
          chrome_policy {
            FindsSettings {
              FindsSettings: 2
            }
          }
        })");
    case ModelBasedCapabilityKey::kAnnotationReducerOnePResolver:
      // TODO(crbug.com/487416734): Add network traffic annotation.
      return MISSING_TRAFFIC_ANNOTATION;
    case ModelBasedCapabilityKey::kAnnotationReducerQueryClassifier:
      // TODO(crbug.com/492168146): Add network traffic annotation.
      return MISSING_TRAFFIC_ANNOTATION;
    case ModelBasedCapabilityKey::kContextualCueing:
      return net::DefineNetworkTrafficAnnotation("suggestions_powered_by_ai",
                                                 R"(
        semantics {
          sender: "Suggestions, powered by AI"
          description:
            "Generates contextual suggestions to the user at relevant moments."
          trigger:
            "User navigates to a page that is a part of one of the target "
            "CUJs (ex: shopping and education)."
          destination: GOOGLE_OWNED_SERVICE
          data:
            "Title and URL of the main frame page the user has navigated too."
          internal {
            contacts {
              email: "sophiechang@google.com"
            }
          }
          user_data {
            type: SENSITIVE_URL
            type: WEB_CONTENT
          }
          last_reviewed: "2026-04-28"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Administrators can control this feature via the "
            "ChromeSuggestionsSettings policy. Users must also have history "
            "sync enabled to use this feature."
          chrome_policy {
            ChromeSuggestionsSettings {
              ChromeSuggestionsSettings: 1
            }
          }
        })");
    case ModelBasedCapabilityKey::kUpdaterChat:
      // TODO(crbug.com/512194219): Add network traffic annotation.
      return MISSING_TRAFFIC_ANNOTATION;
    case ModelBasedCapabilityKey::kCardRecommendations:
      // TODO(crbug.com/534406694): Add network traffic annotation.
      return MISSING_TRAFFIC_ANNOTATION;
    case ModelBasedCapabilityKey::kContextHub:
      // TODO(crbug.com/532549697): Add network traffic annotation.
      return MISSING_TRAFFIC_ANNOTATION;
    case ModelBasedCapabilityKey::kReadAloudGenerateText:
      return net::DefineNetworkTrafficAnnotation("read_aloud_generate_text", R"(
        semantics {
          sender: "Read Aloud AI Playback Text Generation"
          description:
            "Generates conversational, distilled text from a webpage for "
            "the Read Aloud conversational audio playback feature."
          trigger:
            "User triggers the Read Aloud conversational playback mode."
          destination: GOOGLE_OWNED_SERVICE
          data:
            "The webpage title, main distilled content segments, and "
            "sanitized page URL."
          internal {
            contacts {
              email: "ericchao@google.com"
            }
            contacts {
              email: "andresmolina@google.com"
            }
          }
          user_data {
            type: SENSITIVE_URL
            type: WEB_CONTENT
          }
          last_reviewed: "2026-07-10"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled directly by a settings toggle. "
            "It is enabled for users who have 'Make searches and browsing "
            "better' enabled in Chrome settings."
          chrome_policy {
            UrlKeyedAnonymizedDataCollectionEnabled {
              policy_options {mode: MANDATORY}
              UrlKeyedAnonymizedDataCollectionEnabled: false
            }
          }
        })");
    case ModelBasedCapabilityKey::kReadAloudSynthesize:
      return net::DefineNetworkTrafficAnnotation("read_aloud_synthesize", R"(
        semantics {
          sender: "Read Aloud AI Playback Speech Synthesis"
          description:
            "Sends text chunks to Google servers to synthesize and stream back "
            "audio speech bytes for Read Aloud playback."
          trigger:
            "User plays or seeks in Read Aloud playback mode, or when "
            "pre-fetching the next audio segment."
          destination: GOOGLE_OWNED_SERVICE
          data:
            "The text chunk to be synthesized, target voice identifier, and "
            "BCP-47 language code."
          internal {
            contacts {
              email: "ericchao@google.com"
            }
            contacts {
              email: "andresmolina@google.com"
            }
          }
          user_data {
            type: WEB_CONTENT
          }
          last_reviewed: "2026-07-10"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled directly by a settings toggle. "
            "It is enabled for users who have 'Make searches and browsing "
            "better' enabled in Chrome settings."
          chrome_policy {
            UrlKeyedAnonymizedDataCollectionEnabled {
              policy_options {mode: MANDATORY}
              UrlKeyedAnonymizedDataCollectionEnabled: false
            }
          }
        })");
  }
}

void AppendHeadersIfNeeded(network::ResourceRequest& request) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kModelExecutionEnableRemoteDebugLoggingSwitch)) {
    return;
  }
  request.headers.SetHeaderIfMissing(
      kOptimizationGuideModelExecutionDebugLogsHeaderKey, "");
}

bool IsAccessTokenRequiredForFeature(ModelBasedCapabilityKey feature) {
  switch (feature) {
    case ModelBasedCapabilityKey::kCompose:
    case ModelBasedCapabilityKey::kWallpaperSearch:
    case ModelBasedCapabilityKey::kTest:
    case ModelBasedCapabilityKey::kHistorySearch:
    case ModelBasedCapabilityKey::kBlingPrototyping:
    case ModelBasedCapabilityKey::kEnhancedCalendar:
    case ModelBasedCapabilityKey::kZeroStateSuggestions:
    case ModelBasedCapabilityKey::kWalletablePassExtraction:
    case ModelBasedCapabilityKey::kIosSmartTabGrouping:
    case ModelBasedCapabilityKey::kSkills:
    case ModelBasedCapabilityKey::kContentAnnotation:
    case ModelBasedCapabilityKey::kFinds:
    case ModelBasedCapabilityKey::kAnnotationReducerOnePResolver:
    case ModelBasedCapabilityKey::kAnnotationReducerQueryClassifier:
    case ModelBasedCapabilityKey::kContextualCueing:
    case ModelBasedCapabilityKey::kUpdaterChat:
    case ModelBasedCapabilityKey::kContextHub:
    case ModelBasedCapabilityKey::kReadAloudSynthesize:
      return true;
    case ModelBasedCapabilityKey::kFormsClassifications:
      return !base::FeatureList::IsEnabled(
          features::kOptimizationGuideBypassFormsClassificationAuth);
    case ModelBasedCapabilityKey::kScamDetection:
    case ModelBasedCapabilityKey::kGeminiAntiscamProtection:
    case ModelBasedCapabilityKey::kAmountExtraction:
    case ModelBasedCapabilityKey::kCardRecommendations:
    case ModelBasedCapabilityKey::kReadAloudGenerateText:
      return false;
    case ModelBasedCapabilityKey::kPasswordChangeSubmission:
      return !base::FeatureList::IsEnabled(
          features::kOptimizationGuideBypassPasswordChangeAuth);
  }
}

}  // namespace optimization_guide
