// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/server/bloom_server_proxy_impl.h"

#include <memory>

#include "base/base64.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/components/bloom/server/bloom_url_loader.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "ui/gfx/image/image.h"

namespace chromeos {
namespace bloom {

namespace {

constexpr char kJsonMimeType[] = "application/json";

std::string EncodeImage(const gfx::Image& image) {
  auto bytes = image.As1xPNGBytes();

  return base::Base64Encode({bytes->front_as<uint8_t>(), bytes->size()});
}

GURL GetUrlWithPath(base::StringPiece path) {
  return GURL(assistant::kBloomServiceUrl + path.as_string());
}

GURL GetCreateImageURL() {
  return GetUrlWithPath(assistant::kBloomCreateImagePath);
}

GURL GetOcrImageURL(base::StringPiece image_id) {
  return GetUrlWithPath(assistant::kBloomOcrImagePath + image_id.as_string());
}

GURL GetSearchProblemURL(base::StringPiece metadata_blob) {
  return GetUrlWithPath(assistant::kBloomSearchProblemPath +
                        metadata_blob.as_string());
}

std::string JSONToString(const base::Value& json) {
  std::string result;
  bool success = base::JSONWriter::Write(json, &result);
  DCHECK(success);
  return result;
}

}  // namespace

class BloomServerProxyImpl::Worker {
 public:
  Worker(BloomServerProxyImpl* parent,
         const std::string& access_token,
         Callback callback)
      : parent_(parent),
        access_token_(access_token),
        callback_(std::move(callback)) {
    DCHECK(parent_);
  }

  ~Worker() {
    if (callback_)
      std::move(callback_).Run(base::nullopt);
  }

  void AnalyzeProblem(const gfx::Image& screenshot) {
    SendCreateImageRequest(screenshot);
  }

 private:
  BloomURLLoader* url_loader() { return parent_->url_loader_.get(); }

  void SendCreateImageRequest(const gfx::Image& screenshot) {
    DVLOG(3) << "Sending create image request to Bloom servers";
    base::Value body{base::Value::Type::DICTIONARY};
    body.SetKey("raw_data", base::Value(EncodeImage(screenshot)));

    SendPostJSONRequest(GetCreateImageURL(), body,
                        base::Bind(&Worker::OnCreateImageResponse,
                                   weak_ptr_factory_.GetWeakPtr()));
  }

  void SendOcrImageRequest(const std::string& image_id) {
    DVLOG(3) << "Sending OCR image request to Bloom servers";
    SendGetRequest(GetOcrImageURL(image_id),
                   base::Bind(&Worker::OnOcrImageResponse,
                              weak_ptr_factory_.GetWeakPtr()));
  }

  void SendSearchProblemRequest(const std::string& metadata_blob) {
    DVLOG(3) << "Sending search problem request to Bloom servers";
    SendGetRequest(GetSearchProblemURL(metadata_blob),
                   base::Bind(&Worker::OnSearchProblemResponse,
                              weak_ptr_factory_.GetWeakPtr()));
  }

  void OnCreateImageResponse(base::Optional<std::string> reply) {
    if (!reply) {
      DVLOG(3) << "Bloom servers responded with ERROR";
      FinishWithError();
      return;
    }

    base::Optional<base::Value> json_reply =
        base::JSONReader::Read(reply.value());
    if (!json_reply) {
      LOG(WARNING) << "Bloom servers responded with invalid JSON";
      FinishWithError();
      return;
    }

    const std::string* image_id = json_reply.value().FindStringKey("imageId");
    if (!image_id) {
      LOG(WARNING) << "'imageId' tag is missing from Bloom server response";
      FinishWithError();
      return;
    }

    DVLOG(3) << "Bloom servers responded with image_id '" << image_id << "'";
    SendOcrImageRequest(*image_id);
  }

  void OnOcrImageResponse(base::Optional<std::string> reply) {
    if (!reply) {
      DVLOG(3) << "Bloom servers responded with ERROR";
      FinishWithError();
      return;
    }

    base::Optional<base::Value> json_reply =
        base::JSONReader::Read(reply.value());
    if (!json_reply) {
      LOG(WARNING) << "Bloom servers responded with invalid JSON";
      FinishWithError();
      return;
    }

    const std::string* metadata_blob =
        json_reply.value().FindStringKey("metadataBlob");
    if (!metadata_blob) {
      LOG(WARNING)
          << "'metadataBlob' tag is missing from Bloom server response.";
      FinishWithError();
      return;
    }

    LOG(WARNING) << "Bloom servers responded with valid response";
    SendSearchProblemRequest(*metadata_blob);
  }

  void OnSearchProblemResponse(base::Optional<std::string> reply) {
    FinishWithReply(std::move(reply));
  }

  void SendPostJSONRequest(const GURL& url,
                           const base::Value& json,
                           Callback callback) {
    url_loader()->SendPostRequest(url, access_token_, JSONToString(json),
                                  kJsonMimeType, std::move(callback));
  }

  void SendGetRequest(const GURL& url, Callback callback) {
    url_loader()->SendGetRequest(url, access_token_, std::move(callback));
  }

  void FinishWithError() { FinishWithReply(base::nullopt); }

  void FinishWithReply(base::Optional<std::string> reply) {
    std::move(callback_).Run(std::move(reply));
  }

  BloomServerProxyImpl* const parent_;
  const std::string access_token_;
  Callback callback_;

  base::WeakPtrFactory<Worker> weak_ptr_factory_{this};
};

BloomServerProxyImpl::BloomServerProxyImpl(
    std::unique_ptr<BloomURLLoader> url_loader)
    : url_loader_(std::move(url_loader)) {
  DCHECK(url_loader_);
}

BloomServerProxyImpl::~BloomServerProxyImpl() = default;

void BloomServerProxyImpl::AnalyzeProblem(const std::string& access_token,
                                          const gfx::Image& screenshot,
                                          Callback callback) {
  worker_ = std::make_unique<Worker>(this, access_token, std::move(callback));
  worker_->AnalyzeProblem(screenshot);
}

}  // namespace bloom
}  // namespace chromeos
