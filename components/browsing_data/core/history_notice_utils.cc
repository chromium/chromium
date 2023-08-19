// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/history_notice_utils.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/version_info/version_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {

// Merges several asynchronous boolean callbacks into one that returns a boolean
// product of their responses. Deletes itself when done.
class MergeBooleanCallbacks {
 public:
  // Constructor. Upon receiving |expected_call_count| calls to |RunCallback|,
  // |target_callback| will be run with the boolean product of their results.
  MergeBooleanCallbacks(int expected_call_count,
                        base::OnceCallback<void(bool)> target_callback)
      : expected_call_count_(expected_call_count),
        target_callback_(std::move(target_callback)),
        final_response_(true),
        call_count_(0) {}

  // This method is to be bound to all asynchronous callbacks which we want
  // to merge.
  void RunCallback(bool response) {
    final_response_ &= response;

    if (++call_count_ < expected_call_count_)
      return;

    std::move(target_callback_).Run(final_response_);
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                  this);
  }

 private:
  int expected_call_count_;
  base::OnceCallback<void(bool)> target_callback_;
  bool final_response_;
  int call_count_;
};

}  // namespace

namespace browsing_data {

void ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
    const syncer::SyncService* sync_service,
    history::WebHistoryService* history_service,
    base::OnceCallback<void(bool)> callback) {
  if (!sync_service ||
      !sync_service->GetActiveDataTypes().Has(
          syncer::HISTORY_DELETE_DIRECTIVES) ||
      sync_service->GetUserSettings()->IsUsingExplicitPassphrase() ||
      !history_service) {
    std::move(callback).Run(false);
    return;
  }
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation("history_notice_utils_notice",
                                                 "web_history_service", R"(
      semantics {
        description:
          "Queries history.google.com to find out if user has the 'Include "
          "Chrome browsing history and activity from websites and apps that "
          "use Google services' option enabled in the Activity controls of "
          "their Google account. This is done for users who sync their "
          "browsing history without a custom passphrase in order to show "
          "information about history.google.com on the history page and in "
          "the Clear Browsing Data dialog."
        trigger:
          "This request is sent when user opens the history page or the "
          "Clear Browsing Data dialog and history sync without a custom "
          "passphrase is (re)enabled."
        data:
          "An OAuth2 token authenticating the user."
      }
      policy {
        chrome_policy {
          SyncDisabled {
            SyncDisabled: true
          }
        }
      })");
  history_service->QueryWebAndAppActivity(std::move(callback),
                                          partial_traffic_annotation);
}

void ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
    const syncer::SyncService* sync_service,
    history::WebHistoryService* history_service,
    version_info::Channel channel,
    base::OnceCallback<void(bool)> callback) {
  if (!sync_service ||
      !sync_service->GetActiveDataTypes().Has(
          syncer::HISTORY_DELETE_DIRECTIVES) ||
      sync_service->GetUserSettings()->IsUsingExplicitPassphrase() ||
      !history_service) {
    std::move(callback).Run(false);
    return;
  }

  // Return the boolean product of QueryWebAndAppActivity and
  // QueryOtherFormsOfBrowsingHistory. MergeBooleanCallbacks deletes itself
  // after processing both callbacks.
  MergeBooleanCallbacks* merger =
      new MergeBooleanCallbacks(2, std::move(callback));

  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation("history_notice_utils_popup",
                                                 "web_history_service", R"(
          semantics {
            description:
              "Determines if the user has other forms of browsing history "
              "(than Chrome browsing history) stored in their Google account. "
              "This is used to inform the users about the existence of other "
              "forms of browsing history when they delete their Chrome "
              "browsing history from the Clear Browsing Data dialog."
            trigger:
              "This request is sent when user opens the Clear Browsing Data "
              "dialog and history sync without a custom passphrase is "
              "(re)enabled."
            data: "An OAuth2 token authenticating the user."
          }
          policy {
            chrome_policy {
              SyncDisabled {
                SyncDisabled: true
              }
            }
          })");
  history_service->QueryWebAndAppActivity(
      base::BindOnce(&MergeBooleanCallbacks::RunCallback,
                     base::Unretained(merger)),
      partial_traffic_annotation);
  history_service->QueryOtherFormsOfBrowsingHistory(
      channel,
      base::BindOnce(&MergeBooleanCallbacks::RunCallback,
                     base::Unretained(merger)),
      partial_traffic_annotation);
}

}  // namespace browsing_data
