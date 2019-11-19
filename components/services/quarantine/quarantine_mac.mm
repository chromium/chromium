// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/quarantine.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/services/quarantine/common.h"
#include "components/services/quarantine/common_mac.h"
#include "url/gurl.h"

namespace {

bool SetQuarantineProperties(const base::FilePath& file,
                             NSDictionary* properties) {
  base::scoped_nsobject<NSURL> file_url([[NSURL alloc]
      initFileURLWithPath:base::SysUTF8ToNSString(file.value())]);
  if (!file_url)
    return false;

  NSError* error = nil;
  bool success = [file_url setResourceValue:properties
                                     forKey:NSURLQuarantinePropertiesKey
                                      error:&error];
  if (!success) {
    std::string error_message(error ? error.description.UTF8String : "");
    LOG(WARNING) << "Unable to set quarantine attributes on file "
                 << file.value() << ". Error: " << error_message;
    return false;
  }
  return true;
}

}  // namespace

namespace quarantine {

namespace {

// As of Mac OS X 10.4 ("Tiger"), files can be tagged with metadata describing
// various attributes.  Metadata is integrated with the system's Spotlight
// feature and is searchable.  Ordinarily, metadata can only be set by
// Spotlight importers, which requires that the importer own the target file.
// However, there's an attribute intended to describe the origin of a
// file, that can store the source URL and referrer of a downloaded file.
// It's stored as a "com.apple.metadata:kMDItemWhereFroms" extended attribute,
// structured as a binary1-format plist containing a list of sources.  This
// attribute can only be populated by the downloader, not a Spotlight importer.
// Safari on 10.4 and later populates this attribute.
//
// With this metadata set, you can locate downloads by performing a Spotlight
// search for their source or referrer URLs, either from within the Spotlight
// UI or from the command line:
//     mdfind 'kMDItemWhereFroms == "http://releases.mozilla.org/*"'
//
// There is no documented API to set metadata on a file directly as of the
// 10.5 SDK.  The MDSetItemAttribute function does exist to perform this task,
// but it's undocumented.
//
// Note that the Metadata.framework in CoreServices has been superseded by the
// NSMetadata API (e.g. kMDItemWhereFroms -> NSMetadataItemWhereFromsKey, etc).
// The NSMetadata API still is a query-only interface, with no way to set
// attributes, so we continue to use the original API.
bool AddOriginMetadataToFile(const base::FilePath& file,
                             const GURL& source,
                             const GURL& referrer) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // There's no declaration for MDItemSetAttribute in any known public SDK.
  // It exists in the 10.4 and 10.5 runtimes.  To play it safe, do the lookup
  // at runtime instead of declaring it ourselves and linking against what's
  // provided.  This has two benefits:
  //  - If Apple relents and declares the function in a future SDK (it's
  //    happened before), our build won't break.
  //  - If Apple removes or renames the function in a future runtime, the
  //    loader won't refuse to let the application launch.  Instead, we'll
  //    silently fail to set any metadata.
  using MDItemSetAttribute_type =
      OSStatus (*)(MDItemRef, CFStringRef, CFTypeRef);
  static MDItemSetAttribute_type md_item_set_attribute_func =
      []() -> MDItemSetAttribute_type {
    CFBundleRef metadata_bundle =
        CFBundleGetBundleWithIdentifier(CFSTR("com.apple.Metadata"));
    if (!metadata_bundle)
      return nullptr;

    return reinterpret_cast<MDItemSetAttribute_type>(
        CFBundleGetFunctionPointerForName(metadata_bundle,
                                          CFSTR("MDItemSetAttribute")));
  }();
  if (!md_item_set_attribute_func)
    return false;

  NSString* file_path = base::mac::FilePathToNSString(file);
  if (!file_path)
    return false;

  base::ScopedCFTypeRef<MDItemRef> md_item(
      MDItemCreate(kCFAllocatorDefault, base::mac::NSToCFCast(file_path)));
  if (!md_item) {
    LOG(WARNING) << "MDItemCreate failed for path " << file.value();
    return false;
  }

  // We won't put any more than 2 items into the attribute.
  NSMutableArray* list = [NSMutableArray arrayWithCapacity:2];

  // Follow Safari's lead: the first item in the list is the source URL of the
  // downloaded file. If the referrer is known, store that, too. The URLs may be
  // empty (e.g. files downloaded in Incognito mode); don't add empty URLs.
  NSString* origin_url = base::SysUTF8ToNSString(source.spec());
  if (origin_url && [origin_url length])
    [list addObject:origin_url];
  NSString* referrer_url = base::SysUTF8ToNSString(referrer.spec());
  if (referrer_url && [referrer_url length])
    [list addObject:referrer_url];

  if ([list count]) {
    md_item_set_attribute_func(md_item, kMDItemWhereFroms,
                               base::mac::NSToCFCast(list));
  }

  return true;
}

// Adds quarantine metadata to the file, assuming it has already been
// quarantined by the OS.
// |source| should be the source URL for the download, and |referrer| should be
// the URL the user initiated the download from.

// The OS will automatically quarantine files due to the
// LSFileQuarantineEnabled entry in our Info.plist, but it knows relatively
// little about the files. We add more information about the download to
// improve the UI shown by the OS when the users tries to open the file.
bool AddQuarantineMetadataToFile(const base::FilePath& file,
                                 const GURL& source,
                                 const GURL& referrer) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::scoped_nsobject<NSMutableDictionary> properties;
  bool success = GetQuarantineProperties(file, &properties);

  if (!success)
    return false;

  if (!properties) {
    // If there are no quarantine properties, then the file isn't quarantined
    // (e.g., because the user has set up exclusions for certain file types).
    // We don't want to add any metadata, because that will cause the file to
    // be quarantined against the user's wishes.
    return true;
  }

  // kLSQuarantineAgentNameKey, kLSQuarantineAgentBundleIdentifierKey, and
  // kLSQuarantineTimeStampKey are set for us (see LSQuarantine.h), so we only
  // need to set the values that the OS can't infer.

  if (![properties valueForKey:(NSString*)kLSQuarantineTypeKey]) {
    CFStringRef type = source.SchemeIsHTTPOrHTTPS()
                           ? kLSQuarantineTypeWebDownload
                           : kLSQuarantineTypeOtherDownload;
    [properties setValue:(NSString*)type
                  forKey:(NSString*)kLSQuarantineTypeKey];
  }

  if (![properties valueForKey:(NSString*)kLSQuarantineOriginURLKey] &&
      referrer.is_valid()) {
    NSString* referrer_url = base::SysUTF8ToNSString(referrer.spec());
    [properties setValue:referrer_url
                  forKey:(NSString*)kLSQuarantineOriginURLKey];
  }

  if (![properties valueForKey:(NSString*)kLSQuarantineDataURLKey] &&
      source.is_valid()) {
    NSString* origin_url = base::SysUTF8ToNSString(source.spec());
    [properties setValue:origin_url forKey:(NSString*)kLSQuarantineDataURLKey];
  }

  return SetQuarantineProperties(file, properties);
}

}  // namespace

QuarantineFileResult QuarantineFile(const base::FilePath& file,
                                    const GURL& source_url_unsafe,
                                    const GURL& referrer_url_unsafe,
                                    const std::string& client_guid) {
  if (!base::PathExists(file))
    return QuarantineFileResult::FILE_MISSING;

  GURL source_url = SanitizeUrlForQuarantine(source_url_unsafe);
  GURL referrer_url = SanitizeUrlForQuarantine(referrer_url_unsafe);

  // Don't consider it an error if we fail to add origin metadata.
  AddOriginMetadataToFile(file, source_url, referrer_url);
  bool quarantine_succeeded =
      AddQuarantineMetadataToFile(file, source_url, referrer_url);
  return quarantine_succeeded ? QuarantineFileResult::OK
                              : QuarantineFileResult::ANNOTATION_FAILED;
}

}  // namespace quarantine
