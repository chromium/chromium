// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/request_sender.h"

#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "components/update_client/configurator.h"
#include "components/update_client/network.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"

namespace update_client {

namespace {

// This is an ECDSA prime256v1 named-curve key.
constexpr int kKeyVersion = 15;
constexpr auto kPublicKey = std::to_array<uint8_t>({
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0x82, 0x95, 0xc5, 0xaf, 0xe4, 0xc4, 0x3f, 0x36, 0xdf,
    0x80, 0x6f, 0x05, 0xe6, 0x8b, 0x4a, 0xfa, 0x9c, 0xf5, 0x22, 0x0a, 0xa8,
    0x34, 0xa4, 0xfa, 0xd0, 0x77, 0x7a, 0x09, 0xe5, 0x47, 0xc0, 0xe7, 0x08,
    0x3c, 0x3f, 0xe4, 0xe4, 0x5b, 0x76, 0x82, 0x5d, 0xac, 0xf3, 0xf9, 0x9b,
    0x08, 0x48, 0xe9, 0x02, 0xd0, 0x8f, 0xed, 0x87, 0x9a, 0x9f, 0x52, 0xa0,
    0xc1, 0xb4, 0x3c, 0xa9, 0x54, 0x98, 0xc6,
});

// The content type for all protocol requests.
constexpr char kContentType[] = "application/json";

// Returns the value of |response_cup_server_proof| or the value of
// |response_etag|, if the former value is empty.
const std::string& SelectCupServerProof(
    const std::string& response_cup_server_proof,
    const std::string& response_etag) {
  if (response_cup_server_proof.empty()) {
    DVLOG(3) << "Using etag as cup server proof.";
    return response_etag;
  }
  return response_cup_server_proof;
}

}  // namespace

RequestSender::RequestSender(
    scoped_refptr<NetworkFetcherFactory> fetcher_factory)
    : fetcher_factory_(fetcher_factory), signer_(kKeyVersion, kPublicKey) {}

RequestSender::~RequestSender() = default;

base::OnceClosure RequestSender::Send(
    const std::vector<GURL>& urls,
    const base::flat_map<std::string, std::string>& request_extra_headers,
    const std::string& request_body,
    bool use_signing,
    RequestSenderCallback request_sender_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  urls_ = urls;
  request_extra_headers_ = request_extra_headers;
  request_body_ = request_body;
  use_signing_ = use_signing;
  request_sender_callback_ = std::move(request_sender_callback);

  if (urls_.empty()) {
    HandleSendError(static_cast<int>(ProtocolError::MISSING_URLS), 0);
    return base::DoNothing();
  }

  cur_url_ = urls_.begin();

  SendInternal();
  return base::BindOnce(&RequestSender::Cancel, this);
}

void RequestSender::SendInternal() {
  CHECK(cur_url_ != urls_.end());
  CHECK(cur_url_->is_valid());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GURL url(*cur_url_);
  VLOG(2) << "url: " << url.spec();

  if (use_signing_) {
    std::string request_query_string;
    signer_.SignRequest(request_body_, &request_query_string);
    url = BuildUpdateUrl(url, request_query_string);
  }
  VLOG_IF(2, !url.is_valid()) << "url is not valid.";

  VLOG(2) << "Sending Omaha request: " << request_body_;

  if (!fetcher_factory_) {
    // The request was cancelled.
    return;
  }

  network_fetcher_ = fetcher_factory_->Create();
  if (!network_fetcher_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&RequestSender::SendInternalComplete, this,
                       static_cast<int>(ProtocolError::URL_FETCHER_FAILED),
                       std::string(), std::string(), std::string(),
                       std::string(), 0));
    return;
  }
  network_fetcher_->PostRequest(
      url, request_body_, kContentType, request_extra_headers_,
      base::BindRepeating(&RequestSender::OnResponseStarted, this),
      base::DoNothing(),
      base::BindOnce(&RequestSender::OnNetworkFetcherComplete, this, url));
}

void RequestSender::SendInternalComplete(
    int error,
    const std::string& response_body,
    const std::string& response_etag,
    const std::string& response_cup_server_proof,
    const std::string& response_set_cookie,
    int retry_after_sec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "Omaha response received: " << response_body;
  VLOG_IF(2, error) << "Omaha send error: " << error;

  if (!error) {
    if (!use_signing_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(TakeRequestSenderCallback(), 0,
                                    response_body, retry_after_sec));
      return;
    }

    CHECK(use_signing_);
    if (signer_.ValidateResponse(
            response_body,
            SelectCupServerProof(response_cup_server_proof, response_etag))) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(TakeRequestSenderCallback(), 0,
                                    response_body, retry_after_sec));
      return;
    }

    error = static_cast<int>(ProtocolError::RESPONSE_NOT_TRUSTED);
  }

  CHECK(error);

  // A positive |retry_after_sec| is a hint from the server that the client
  // should not send further request until the cooldown has expired.
  if (retry_after_sec <= 0 && ++cur_url_ != urls_.end() &&
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&RequestSender::SendInternal, this))) {
    return;
  }

  HandleSendError(error, retry_after_sec);
}

void RequestSender::OnResponseStarted(int response_code,
                                      int64_t /*content_length*/) {
  response_code_ = response_code;
}

void RequestSender::OnNetworkFetcherComplete(
    const GURL& original_url,
    std::optional<std::string> response_body,
    int net_error,
    const std::string& header_etag,
    const std::string& xheader_cup_server_proof,
    const std::string& header_set_cookie,
    int64_t xheader_retry_after_sec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << "Request completed from url: " << original_url.spec();

  int error = -1;
  if (!net_error && response_code_ == 200) {
    error = 0;
  } else if (response_code_ != -1) {
    error = response_code_;
  } else {
    error = net_error;
  }

  int retry_after_sec = -1;
  if (original_url.SchemeIsCryptographic() && error >= 0) {
    retry_after_sec = base::saturated_cast<int>(xheader_retry_after_sec);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&RequestSender::SendInternalComplete, this, error,
                     std::move(response_body).value_or(std::string()),
                     header_etag, xheader_cup_server_proof, header_set_cookie,
                     retry_after_sec));
}

void RequestSender::HandleSendError(int error, int retry_after_sec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(TakeRequestSenderCallback(), error,
                                std::string(), retry_after_sec));
}

GURL RequestSender::BuildUpdateUrl(const GURL& url,
                                   const std::string& query_params) {
  const std::string query_string(
      url.has_query() ? base::StringPrintf("%s&%s", url.GetQuery().c_str(),
                                           query_params.c_str())
                      : query_params);
  GURL::Replacements replacements;
  replacements.SetQueryStr(query_string);

  return url.ReplaceComponents(replacements);
}

void RequestSender::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleSendError(static_cast<int>(ServiceError::CANCELLED), 0);
  network_fetcher_.reset();
  fetcher_factory_.reset();
}

RequestSender::RequestSenderCallback
RequestSender::TakeRequestSenderCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__;

  RequestSenderCallback callback = std::move(request_sender_callback_);
  request_sender_callback_ = base::DoNothing();
  return callback;
}

}  // namespace update_client
