// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/media_galleries_custom_bindings.h"

#include <string>

#include "base/functional/bind.h"
#include "extensions/renderer/script_context.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_dom_file_system.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8.h"

namespace extensions {

MediaGalleriesCustomBindings::MediaGalleriesCustomBindings(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void MediaGalleriesCustomBindings::AddRoutes() {
  RouteHandlerFunction(
      "GetMediaFileSystemObject", "mediaGalleries",
      base::BindRepeating(
          &MediaGalleriesCustomBindings::GetMediaFileSystemObject,
          base::Unretained(this)));
}

// FileSystemObject GetMediaFileSystem(string file_system_url): construct
// a file system object from a file system url.
void MediaGalleriesCustomBindings::GetMediaFileSystemObject(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsString());

  std::string fs_mount(*v8::String::Utf8Value(args.GetIsolate(), args[0]));
  CHECK(!fs_mount.empty());

  blink::WebLocalFrame* webframe =
      blink::WebLocalFrame::FrameForCurrentContext();
  const GURL origin =
      url::Origin(webframe->GetDocument().GetSecurityOrigin()).GetURL();
  std::string fs_name =
      storage::GetFileSystemName(origin, storage::kFileSystemTypeExternal);
  fs_name.append("_");
  fs_name.append(fs_mount);
  const GURL root_url(
      storage::GetExternalFileSystemRootURIString(origin, fs_mount));
  args.GetReturnValue().Set(blink::WebDOMFileSystem::Create(
                                webframe, blink::kWebFileSystemTypeExternal,
                                blink::WebString::FromUTF8(fs_name), root_url)
                                .ToV8Value(args.GetIsolate()));
}

}  // namespace extensions
