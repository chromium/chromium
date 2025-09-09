// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/omnibox/composebox/ios/composebox_query_controller_ios.h"

#import <UIKit/UIKit.h>

#include <optional>
#include <vector>

#import "base/task/bind_post_task.h"
#import "base/task/thread_pool.h"
#import "components/lens/lens_bitmap_processing.h"
#import "components/lens/ref_counted_lens_overlay_client_logs.h"
#import "components/omnibox/composebox/ios/composebox_image_helper_ios.h"
#import "third_party/lens_server_proto/lens_overlay_server.pb.h"

void ComposeboxQueryControllerIOS::CreateImageUploadRequest(
    const base::UnguessableToken& file_token,
    const std::vector<uint8_t>& image_data,
    std::optional<lens::ImageEncodingOptions> image_options,
    RequestBodyProtoCreatedCallback callback) {
  FileInfo* file_info = GetFileInfo(file_token);
  if (!file_info) {
    return;
  }

  CHECK(image_options.has_value());
  // On iOS, we use UIImage for decoding and resizing.
  NSData* image_ns_data = [NSData dataWithBytes:image_data.data()
                                         length:image_data.size()];
  UIImage* image = [UIImage imageWithData:image_ns_data];

  if (!image) {
    std::move(callback).Run(lens::LensOverlayServerRequest(),
                            FileUploadErrorType::kImageProcessingError);
    return;
  }

  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();

  // Downscaling and encoding is done on a background thread.
  create_request_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&composebox::DownscaleAndEncodeImage, image,
                     ref_counted_logs, image_options.value()),
      base::BindOnce(&ComposeboxQueryController::
                         CreateFileUploadRequestProtoWithImageDataAndContinue,
                     *file_info->request_id_, CreateClientContext(),
                     ref_counted_logs, std::move(callback)));
}
