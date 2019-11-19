// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_ROUTER_MEDIA_SOURCE_H_
#define CHROME_COMMON_MEDIA_ROUTER_MEDIA_SOURCE_H_

#include <iosfwd>
#include <string>

#include "base/hash/hash.h"
#include "url/gurl.h"

namespace media_router {

// URL schemes used by Presentation URLs for Cast and DIAL.
constexpr char kCastPresentationUrlScheme[] = "cast";
constexpr char kCastDialPresentationUrlScheme[] = "cast-dial";
constexpr char kDialPresentationUrlScheme[] = "dial";
constexpr char kRemotePlaybackPresentationUrlScheme[] = "remote-playback";

// URL prefix used by legacy Cast presentations.
constexpr char kLegacyCastPresentationUrlPrefix[] =
    "https://google.com/cast#__castAppId__=";

// Strings used in presentation IDs by the Cast SDK implementation.
// TODO(takumif): Move them out of this file, since they are not directly
// related to MediaSource.
//
// This value must be the same as |chrome.cast.AUTO_JOIN_PRESENTATION_ID| in the
// component extension.
constexpr char kAutoJoinPresentationId[] = "auto-join";
// This value must be the same as |chrome.cast.PRESENTATION_ID_PREFIX| in the
// component extension.
constexpr char kCastPresentationIdPrefix[] = "cast-session_";

// Returns true if |url| represents a legacy Cast presentation URL, i.e., it
// starts with |kLegacyCastPresentationUrlPrefix|.
bool IsLegacyCastPresentationUrl(const GURL& url);

// Returns true if |url| is a valid presentation URL.
bool IsValidPresentationUrl(const GURL& url);

// Returns true if |presentation_id| is an ID used by auto-join requests.
bool IsAutoJoinPresentationId(const std::string& presentation_id);

class MediaSource {
 public:
  using Id = std::string;

  MediaSource();
  explicit MediaSource(const MediaSource::Id& id);
  explicit MediaSource(const GURL& presentation_url);
  ~MediaSource();

  // Gets the ID of the media source.
  const Id& id() const { return id_; }

  // If MediaSource is created from a URL, return the URL; otherwise return an
  // empty GURL.
  const GURL& url() const { return url_; }

  // Returns true if two MediaSource objects use the same media ID.
  bool operator==(const MediaSource& other) const { return id_ == other.id(); }

  bool operator<(const MediaSource& other) const { return id_ < other.id(); }

  // Hash operator for hash containers.
  struct Hash {
    uint32_t operator()(const MediaSource& source) const {
      return base::Hash(source.id());
    }
  };

  // Protocol-specific media source object creation.
  // Returns MediaSource URI depending on the type of source.
  static MediaSource ForTab(int tab_id);
  static MediaSource ForTabContentRemoting(int tab_id);
  static MediaSource ForPresentationUrl(const GURL& presentation_url);

  // Creates a media source for a specific desktop.
  static MediaSource ForDesktop(const std::string& desktop_media_id);

  // Creates a media source representing "the" desktop.  When possible, prefer
  // the form with an argument instead.  This type of source is used to
  // represent using a desktop as a media source at a point where, in the case
  // of multiple desktops, the actual desktop to use cannot be determined.
  // Before a route can be created using the source, the desktop picker must be
  // invoked to choose a specific desktop.
  //
  // TODO(crbug.com/809249): See if this method can be removed after the
  // extension-based Cast MRP is removed.
  static MediaSource ForDesktop();

  // Returns true if source outputs its content via mirroring.
  bool IsDesktopMirroringSource() const;
  bool IsTabMirroringSource() const;
  bool IsMirroringSource() const;

  // Returns true if this is represents a Cast Presentation URL.
  bool IsCastPresentationUrl() const;

  // Parses the ID and returns the SessionTabHelper tab ID referencing a source
  // tab. Returns a non-positive value on error.
  int TabId() const;

  // When this source was created by ForDesktop(string), returns a stream ID
  // suitable for passing to
  // content::DesktopStreamsRegistry::RequestMediaForStreamId().  Otherwise
  // returns base::nullopt.
  base::Optional<std::string> DesktopStreamId() const;

  // Checks that this is a parseable URN and is of a known type.
  // Does not deeper protocol-level syntax checks.
  bool IsValid() const;

  // Returns true this source outputs its content via DIAL.
  // TODO(crbug.com/804419): Move this to in-browser DIAL/Cast MRP when we have
  // one.
  bool IsDialSource() const;

  // Returns empty string if this source is not DIAL media source, or is not a
  // valid DIAL media source.
  std::string AppNameFromDialSource() const;

 private:
  MediaSource::Id id_;
  GURL url_;
};

// Only for debug logging.  This operator is defined inline so it doesn't add
// any code in release builds.  (Omitting the definition entirely when NDEBUG is
// defined causes linker errors on Android.)
inline std::ostream& operator<<(std::ostream& stream,
                                const MediaSource& source) {
  return stream << "MediaSource[" << source.id() << "]";
}

}  // namespace media_router

#endif  // CHROME_COMMON_MEDIA_ROUTER_MEDIA_SOURCE_H_
