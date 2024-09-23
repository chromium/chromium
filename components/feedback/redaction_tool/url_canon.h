// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This is a copy of url/url_canon.h circa 2023. It should be used only by
// components/feedback/redaction_tool/.
// We need a copy because the components/feedback/redaction_tool source code is
// shared into ChromeOS and needs to have no dependencies outside of base/.

#ifndef COMPONENTS_FEEDBACK_REDACTION_TOOL_URL_CANON_H_
#define COMPONENTS_FEEDBACK_REDACTION_TOOL_URL_CANON_H_

#include <stdlib.h>
#include <string.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/numerics/clamped_math.h"
#include "components/feedback/redaction_tool/url_parse.h"

namespace redaction_internal {

// Canonicalizer output -------------------------------------------------------

// Base class for the canonicalizer output, this maintains a buffer and
// supports simple resizing and append operations on it.
//
// It is VERY IMPORTANT that no virtual function calls be made on the common
// code path. We only have two virtual function calls, the destructor and a
// resize function that is called when the existing buffer is not big enough.
// The derived class is then in charge of setting up our buffer which we will
// manage.
template <typename T>
class CanonOutputT {
 public:
  CanonOutputT() = default;
  virtual ~CanonOutputT() = default;

  // Implemented to resize the buffer. This function should update the buffer
  // pointer to point to the new buffer, and any old data up to |cur_len_| in
  // the buffer must be copied over.
  //
  // The new size |sz| must be larger than buffer_len_.
  virtual void Resize(size_t sz) = 0;

  // Accessor for returning a character at a given position. The input offset
  // must be in the valid range.
  inline T at(size_t offset) const { return buffer_[offset]; }

  // Sets the character at the given position. The given position MUST be less
  // than the length().
  inline void set(size_t offset, T ch) { buffer_[offset] = ch; }

  // Returns the number of characters currently in the buffer.
  inline size_t length() const { return cur_len_; }

  // Returns the current capacity of the buffer. The length() is the number of
  // characters that have been declared to be written, but the capacity() is
  // the number that can be written without reallocation. If the caller must
  // write many characters at once, it can make sure there is enough capacity,
  // write the data, then use set_size() to declare the new length().
  size_t capacity() const { return buffer_len_; }

  // Called by the user of this class to get the output. The output will NOT
  // be NULL-terminated. Call length() to get the
  // length.
  const T* data() const { return buffer_; }
  T* data() { return buffer_; }

  // Shortens the URL to the new length. Used for "backing up" when processing
  // relative paths. This can also be used if an external function writes a lot
  // of data to the buffer (when using the "Raw" version below) beyond the end,
  // to declare the new length.
  //
  // This MUST NOT be used to expand the size of the buffer beyond capacity().
  void set_length(size_t new_len) { cur_len_ = new_len; }

  // This is the most performance critical function, since it is called for
  // every character.
  void push_back(T ch) {
    // In VC2005, putting this common case first speeds up execution
    // dramatically because this branch is predicted as taken.
    if (cur_len_ < buffer_len_) {
      buffer_[cur_len_] = ch;
      cur_len_++;
      return;
    }

    // Grow the buffer to hold at least one more item. Hopefully we won't have
    // to do this very often.
    if (!Grow(1)) {
      return;
    }

    // Actually do the insertion.
    buffer_[cur_len_] = ch;
    cur_len_++;
  }

  // Appends the given string to the output.
  void Append(const T* str, size_t str_len) {
    if (str_len > buffer_len_ - cur_len_) {
      if (!Grow(str_len - (buffer_len_ - cur_len_))) {
        return;
      }
    }
    memcpy(buffer_ + cur_len_, str, str_len * sizeof(T));
    cur_len_ += str_len;
  }

 protected:
  // Grows the given buffer so that it can fit at least |min_additional|
  // characters. Returns true if the buffer could be resized, false on OOM.
  bool Grow(size_t min_additional) {
    static const size_t kMinBufferLen = 16;
    size_t new_len = (buffer_len_ == 0) ? kMinBufferLen : buffer_len_;
    do {
      if (new_len >= (1 << 30)) {  // Prevent overflow below.
        return false;
      }
      new_len *= 2;
    } while (new_len < buffer_len_ + min_additional);
    Resize(new_len);
    return true;
  }

  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION T* buffer_ = nullptr;
  size_t buffer_len_ = 0;

  // Used characters in the buffer.
  size_t cur_len_ = 0;
};

// Simple implementation of the CanonOutput using new[]. This class
// also supports a static buffer so if it is allocated on the stack, most
// URLs can be canonicalized with no heap allocations.
template <typename T, int fixed_capacity = 1024>
class RawCanonOutputT : public CanonOutputT<T> {
 public:
  RawCanonOutputT() : CanonOutputT<T>() {
    this->buffer_ = fixed_buffer_;
    this->buffer_len_ = fixed_capacity;
  }
  ~RawCanonOutputT() override {
    if (this->buffer_ != fixed_buffer_) {
      delete[] this->buffer_;
    }
  }

  void Resize(size_t sz) override {
    T* new_buf = new T[sz];
    memcpy(new_buf, this->buffer_,
           sizeof(T) * (this->cur_len_ < sz ? this->cur_len_ : sz));
    if (this->buffer_ != fixed_buffer_) {
      delete[] this->buffer_;
    }
    this->buffer_ = new_buf;
    this->buffer_len_ = sz;
  }

 protected:
  T fixed_buffer_[fixed_capacity];
};

// Normally, all canonicalization output is in narrow characters. We support
// the templates so it can also be used internally if a wide buffer is
// required.
typedef CanonOutputT<char> CanonOutput;
typedef CanonOutputT<char16_t> CanonOutputW;

template <int fixed_capacity>
class RawCanonOutput : public RawCanonOutputT<char, fixed_capacity> {};
template <int fixed_capacity>
class RawCanonOutputW : public RawCanonOutputT<char16_t, fixed_capacity> {};

// Character set converter ----------------------------------------------------
//
// Converts query strings into a custom encoding. The embedder can supply an
// implementation of this class to interface with their own character set
// conversion libraries.
//
// Embedders will want to see the unit test for the ICU version.

class CharsetConverter {
 public:
  CharsetConverter() = default;
  virtual ~CharsetConverter() = default;

  // Converts the given input string from UTF-16 to whatever output format the
  // converter supports. This is used only for the query encoding conversion,
  // which does not fail. Instead, the converter should insert "invalid
  // character" characters in the output for invalid sequences, and do the
  // best it can.
  //
  // If the input contains a character not representable in the output
  // character set, the converter should append the HTML entity sequence in
  // decimal, (such as "&#20320;") with escaping of the ampersand, number
  // sign, and semicolon (in the previous example it would be
  // "%26%2320320%3B"). This rule is based on what IE does in this situation.
  virtual void ConvertFromUTF16(const char16_t* input,
                                int input_len,
                                CanonOutput* output) = 0;
};

// Schemes --------------------------------------------------------------------

// Types of a scheme representing the requirements on the data represented by
// the authority component of a URL with the scheme.
enum SchemeType {
  // The authority component of a URL with the scheme has the form
  // "username:password@host:port". The username and password entries are
  // optional; the host may not be empty. The default value of the port can be
  // omitted in serialization. This type occurs with network schemes like http,
  // https, and ftp.
  SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION,
  // The authority component of a URL with the scheme has the form "host:port",
  // and does not include username or password. The default value of the port
  // can be omitted in serialization. Used by inner URLs of filesystem URLs of
  // origins with network hosts, from which the username and password are
  // stripped.
  SCHEME_WITH_HOST_AND_PORT,
  // The authority component of an URL with the scheme has the form "host", and
  // does not include port, username, or password. Used when the hosts are not
  // network addresses; for example, schemes used internally by the browser.
  SCHEME_WITH_HOST,
  // A URL with the scheme doesn't have the authority component.
  SCHEME_WITHOUT_AUTHORITY,
};

// This structure holds detailed state exported from the IP/Host canonicalizers.
// Additional fields may be added as callers require them.
struct CanonHostInfo {
  CanonHostInfo() = default;

  // Convenience function to test if family is an IP address.
  bool IsIPAddress() const { return family == IPV4 || family == IPV6; }

  // This field summarizes how the input was classified by the canonicalizer.
  enum Family {
    NEUTRAL,  // - Doesn't resemble an IP address. As far as the IP
              //   canonicalizer is concerned, it should be treated as a
              //   hostname.
    BROKEN,   // - Almost an IP, but was not canonicalized. This could be an
              //   IPv4 address where truncation occurred, or something
              //   containing the special characters :[] which did not parse
              //   as an IPv6 address. Never attempt to connect to this
              //   address, because it might actually succeed!
    IPV4,     // - Successfully canonicalized as an IPv4 address.
    IPV6,     // - Successfully canonicalized as an IPv6 address.
  };
  Family family = NEUTRAL;

  // If |family| is IPV4, then this is the number of nonempty dot-separated
  // components in the input text, from 1 to 4. If |family| is not IPV4,
  // this value is undefined.
  int num_ipv4_components = 0;

  // Location of host within the canonicalized output.
  // CanonicalizeIPAddress() only sets this field if |family| is IPV4 or IPV6.
  // CanonicalizeHostVerbose() always sets it.
  Component out_host;

  // |address| contains the parsed IP Address (if any) in its first
  // AddressLength() bytes, in network order. If IsIPAddress() is false
  // AddressLength() will return zero and the content of |address| is undefined.
  unsigned char address[16];

  // Convenience function to calculate the length of an IP address corresponding
  // to the current IP version in |family|, if any. For use with |address|.
  int AddressLength() const {
    return family == IPV4 ? 4 : (family == IPV6 ? 16 : 0);
  }
};

// Part replacer --------------------------------------------------------------

// Internal structure used for storing separate strings for each component.
// The basic canonicalization functions use this structure internally so that
// component replacement (different strings for different components) can be
// treated on the same code path as regular canonicalization (the same string
// for each component).
//
// A Parsed structure usually goes along with this. Those components identify
// offsets within these strings, so that they can all be in the same string,
// or spread arbitrarily across different ones.
//
// This structures does not own any data. It is the caller's responsibility to
// ensure that the data the pointers point to stays in scope and is not
// modified.
template <typename CHAR>
struct URLComponentSource {
  // Constructor normally used by callers wishing to replace components. This
  // will make them all NULL, which is no replacement. The caller would then
  // override the components they want to replace.
  URLComponentSource()
      : scheme(nullptr),
        username(nullptr),
        password(nullptr),
        host(nullptr),
        port(nullptr),
        path(nullptr),
        query(nullptr),
        ref(nullptr) {}

  // Constructor normally used internally to initialize all the components to
  // point to the same spec.
  explicit URLComponentSource(const CHAR* default_value)
      : scheme(default_value),
        username(default_value),
        password(default_value),
        host(default_value),
        port(default_value),
        path(default_value),
        query(default_value),
        ref(default_value) {}

  raw_ptr<const CHAR> scheme;
  raw_ptr<const CHAR> username;
  raw_ptr<const CHAR> password;
  raw_ptr<const CHAR> host;
  raw_ptr<const CHAR> port;
  raw_ptr<const CHAR> path;
  raw_ptr<const CHAR> query;
  raw_ptr<const CHAR> ref;
};

// This structure encapsulates information on modifying a URL. Each component
// may either be left unchanged, replaced, or deleted.
//
// By default, each component is unchanged. For those components that should be
// modified, call either Set* or Clear* to modify it.
//
// The string passed to Set* functions DOES NOT GET COPIED AND MUST BE KEPT
// IN SCOPE BY THE CALLER for as long as this object exists!
//
// Prefer the 8-bit replacement version if possible since it is more efficient.
template <typename CHAR>
class Replacements {
 public:
  Replacements() = default;

  // Scheme
  void SetScheme(const CHAR* s, const Component& comp) {
    sources_.scheme = s;
    components_.scheme = comp;
  }
  // Note: we don't have a ClearScheme since this doesn't make any sense.
  bool IsSchemeOverridden() const { return sources_.scheme != NULL; }

  // Username
  void SetUsername(const CHAR* s, const Component& comp) {
    sources_.username = s;
    components_.username = comp;
  }
  void ClearUsername() {
    sources_.username = Placeholder();
    components_.username = Component();
  }
  bool IsUsernameOverridden() const { return sources_.username != NULL; }

  // Password
  void SetPassword(const CHAR* s, const Component& comp) {
    sources_.password = s;
    components_.password = comp;
  }
  void ClearPassword() {
    sources_.password = Placeholder();
    components_.password = Component();
  }
  bool IsPasswordOverridden() const { return sources_.password != NULL; }

  // Host
  void SetHost(const CHAR* s, const Component& comp) {
    sources_.host = s;
    components_.host = comp;
  }
  void ClearHost() {
    sources_.host = Placeholder();
    components_.host = Component();
  }
  bool IsHostOverridden() const { return sources_.host != NULL; }

  // Port
  void SetPort(const CHAR* s, const Component& comp) {
    sources_.port = s;
    components_.port = comp;
  }
  void ClearPort() {
    sources_.port = Placeholder();
    components_.port = Component();
  }
  bool IsPortOverridden() const { return sources_.port != NULL; }

  // Path
  void SetPath(const CHAR* s, const Component& comp) {
    sources_.path = s;
    components_.path = comp;
  }
  void ClearPath() {
    sources_.path = Placeholder();
    components_.path = Component();
  }
  bool IsPathOverridden() const { return sources_.path != NULL; }

  // Query
  void SetQuery(const CHAR* s, const Component& comp) {
    sources_.query = s;
    components_.query = comp;
  }
  void ClearQuery() {
    sources_.query = Placeholder();
    components_.query = Component();
  }
  bool IsQueryOverridden() const { return sources_.query != NULL; }

  // Ref
  void SetRef(const CHAR* s, const Component& comp) {
    sources_.ref = s;
    components_.ref = comp;
  }
  void ClearRef() {
    sources_.ref = Placeholder();
    components_.ref = Component();
  }
  bool IsRefOverridden() const { return sources_.ref != NULL; }

  // Getters for the internal data. See the variables below for how the
  // information is encoded.
  const URLComponentSource<CHAR>& sources() const { return sources_; }
  const Parsed& components() const { return components_; }

 private:
  // Returns a pointer to a static empty string that is used as a placeholder
  // to indicate a component should be deleted (see below).
  const CHAR* Placeholder() {
    static const CHAR empty_cstr = 0;
    return &empty_cstr;
  }

  // We support three states:
  //
  // Action                 | Source                Component
  // -----------------------+--------------------------------------------------
  // Don't change component | NULL                  (unused)
  // Replace component      | (replacement string)  (replacement component)
  // Delete component       | (non-NULL)            (invalid component: (0,-1))
  //
  // We use a pointer to the empty string for the source when the component
  // should be deleted.
  URLComponentSource<CHAR> sources_;
  Parsed components_;
};

}  // namespace redaction_internal

#endif  // COMPONENTS_FEEDBACK_REDACTION_TOOL_URL_CANON_H_
