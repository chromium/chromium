// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! **Rust API for crash keys.**
//!
//! A crash key is an annotation that is carried along with a crash report, to
//! provide additional debugging information beyond a stack trace. Crash keys
//! have a name and a string value. See `//docs/debugging_with_crash_keys.md`.
//!
//! This is the Rust equivalent of C++'s `crash_reporter::CrashKeyString<N>` in
//! `components/crash/core/common/crash_key.h` (which is itself an alias for
//! //third_party/crashpad's StringAnnotation), and is laid out identically in
//! memory.
//!
//! Crash keys must be declared as `static`s:
//!
//! ```
//! chromium::import! {
//!     "//components/crash/core/common:crash_key_rust";
//! }
//!
//! use crash_key_rust::CrashKeyString;
//!
//! static SOME_DATA_TO_LOG: CrashKeyString<256> = CrashKeyString::new(c"some_data_to_log");
//!
//! fn on_failure(data: &str) {
//!     SOME_DATA_TO_LOG.set(data);
//!     // ... later, e.g. once the crash report has been triggered:
//!     SOME_DATA_TO_LOG.clear();
//! }
//! ```

use std::cell::UnsafeCell;
use std::ffi::{c_char, c_void, CStr};
use std::fmt::{self, Display};
use std::sync::atomic::{AtomicBool, AtomicPtr, AtomicU32, Ordering};

chromium::import! {
    "//components/crash/core/common:crash_key_rust_crashpad_ffi" as ffi;
}

/// Mirrors the memory layout of //third_party/crashpad's `Annotation` (see
/// `third_party/crashpad/crashpad/client/annotation.h`). Crashpad reads this
/// both in- and out-of-process so the match must be identical.
// LINT.IfChange(AnnotationLayout)
#[repr(C)]
struct AnnotationHeader {
    link_node: AtomicPtr<c_void>,
    name: *const c_char,
    value_ptr: AtomicPtr<c_void>,
    size: AtomicU32,
    annotation_type: u16,
    concurrent_access_guard_mode: bool,
    spin_guard_state: AtomicBool,
}
// LINT.ThenChange(//third_party/crashpad/crashpad/client/annotation.h)

/// The size of each of the pointer fields at the start of `AnnotationHeader`
/// (`link_node`, `name`, and `value_ptr`).
const ANNOTATION_POINTER_FIELD_SIZE: usize = size_of::<usize>();

/// Asserts to enforce that layouts are identical.
const _: () = {
    assert!(align_of::<AnnotationHeader>() == align_of::<usize>());
    assert!(size_of::<AnnotationHeader>() == 3 * ANNOTATION_POINTER_FIELD_SIZE + 8);
    assert!(std::mem::offset_of!(AnnotationHeader, link_node) == 0);
    assert!(std::mem::offset_of!(AnnotationHeader, name) == ANNOTATION_POINTER_FIELD_SIZE);
    assert!(std::mem::offset_of!(AnnotationHeader, value_ptr) == 2 * ANNOTATION_POINTER_FIELD_SIZE);
    assert!(std::mem::offset_of!(AnnotationHeader, size) == 3 * ANNOTATION_POINTER_FIELD_SIZE);
    assert!(
        std::mem::offset_of!(AnnotationHeader, annotation_type)
            == 3 * ANNOTATION_POINTER_FIELD_SIZE + 4
    );
    assert!(
        std::mem::offset_of!(AnnotationHeader, concurrent_access_guard_mode)
            == 3 * ANNOTATION_POINTER_FIELD_SIZE + 6
    );
    assert!(
        std::mem::offset_of!(AnnotationHeader, spin_guard_state)
            == 3 * ANNOTATION_POINTER_FIELD_SIZE + 7
    );
};

/// `crashpad::Annotation::Type::kString`.
const ANNOTATION_TYPE_STRING: u16 = 1;

/// The maximum length of a crash key name, excluding the trailing NUL.
///
/// This matches the compile-time check in //base/debug/crash_logging.h, which
/// restricts the name of a crash key to fewer than 40 characters.
// LINT.IfChange(MaxCrashKeyNameLength)
const MAX_NAME_LENGTH: usize = 39;
// LINT.ThenChange(//base/debug/crash_logging.cc:MaxCrashKeyNameLength)

const fn is_valid_crash_key_name(name: &CStr) -> bool {
    let bytes = name.to_bytes();
    if bytes.len() > MAX_NAME_LENGTH {
        return false;
    }
    // Using a simple while loop instead of an iterator because iterators can't
    // be used in a `const fn`.
    let mut i = 0;
    while i < bytes.len() {
        // The `:` character is reserved.
        if bytes[i] == b':' {
            return false;
        }
        i += 1;
    }
    true
}

/// A crash key with room for up to `N` bytes.
/// Rust equivalent of `crash_reporter::CrashKeyString<N>`.
///
/// `N` must be one of the sizes supported by `base::debug::CrashKeySize`
/// (32, 64, 256, or 1024).
///
/// `CrashKeyString`s must be declared as `static`s since Crashpad keeps a
/// pointer to them once they are set.
#[repr(C)]
pub struct CrashKeyString<const N: usize> {
    header: AnnotationHeader,
    value: UnsafeCell<[u8; N]>,
    name: &'static CStr,
    /// Held while writing to `value` (and the corresponding `header.size`).
    writer_lock: AtomicBool,
}

impl<const N: usize> CrashKeyString<N> {
    pub const fn new(name: &'static CStr) -> Self {
        assert!(
            matches!(N, 32 | 64 | 256 | 1024),
            "Crash key sizes must be one of 32, 64, 256, or 1024."
        );
        assert!(
            is_valid_crash_key_name(name),
            "Crash key names must be shorter than 40 characters and must not contain ':'."
        );
        Self {
            header: AnnotationHeader {
                link_node: AtomicPtr::new(std::ptr::null_mut()),
                name: name.as_ptr(),
                value_ptr: AtomicPtr::new(std::ptr::null_mut()),
                size: AtomicU32::new(0),
                annotation_type: ANNOTATION_TYPE_STRING,
                concurrent_access_guard_mode: false,
                spin_guard_state: AtomicBool::new(false),
            },
            value: UnsafeCell::new([0; N]),
            name,
            writer_lock: AtomicBool::new(false),
        }
    }

    pub fn name(&self) -> &'static CStr {
        self.name
    }

    /// Stores the `Display` representation of `value` in the crash key, e.g.
    /// `KEY.set("text")`, `KEY.set(42)`, or
    /// `KEY.set(format_args!("{id}:{name}"))`.
    ///
    /// # Thread safety
    ///
    /// Can be called from any thread. If another thread is writing to the same
    /// crash key at the same time, this write is dropped rather than waiting,
    /// so setting a crash key never blocks.
    pub fn set(&'static self, value: impl Display) {
        let Some(_lock) = self.try_lock_writer() else {
            return;
        };
        // SAFETY: `writer_lock` is held, so nothing else is writing to
        // `self.value`.
        let buffer = unsafe { &mut *self.value.get() };
        let mut writer = TruncatingWriter { buffer, len: 0 };
        // An error just means that the value was truncated.
        let _ = fmt::write(&mut writer, format_args!("{value}"));
        let len = writer.len;
        debug_assert!(
            !buffer[..len].contains(&0),
            "Crash key values must not contain NUL characters."
        );
        // `len` is at most `N`, which is at most 1024.
        self.header.size.store(len as u32, Ordering::Release);
        self.register();
    }

    /// Can be called from any thread; see `set()` for details.
    pub fn clear(&'static self) {
        let Some(_lock) = self.try_lock_writer() else {
            return;
        };
        self.header.size.store(0, Ordering::Release);
    }

    fn register(&'static self) {
        if !self.header.link_node.load(Ordering::Acquire).is_null() {
            return;
        }
        // `crashpad::Annotation::value_ptr_` is set lazily because a `const fn`
        // can't refer to the address of the `static` it's initializing. This is
        // fine since Crashpad only reads `value_ptr_` of annotations in its
        // list. The registration below synchronizes with readers of the list.
        self.header.value_ptr.store(self.value.get().cast(), Ordering::Relaxed);
        // SAFETY: `self.header` is layout-compatible with
        // `crashpad::Annotation`, is fully initialized, and is `'static`.
        unsafe {
            ffi::RegisterAnnotation(std::ptr::from_ref(&self.header).cast_mut().cast());
        }
    }

    // Writes to the same crash key are serialized by `writer_lock`. If another
    // thread holds it, the write is dropped rather than waiting for the lock.
    // This is equivalent to the dropped write happening first and then being
    // overwritten by the other write, and ensures that setting a crash key
    // never blocks. (In C++, concurrent writes to the same crash key are a data
    // race, which has caused TSan failures in the past.)
    fn try_lock_writer(&self) -> Option<WriterLockGuard<'_>> {
        self.writer_lock
            .compare_exchange(false, true, Ordering::Acquire, Ordering::Relaxed)
            .ok()
            .map(|_| WriterLockGuard { lock: &self.writer_lock })
    }

    /// Returns a pointer to this crash key's storage, to be treated as a
    /// `crashpad::Annotation*`. Only available in test-only builds of this
    /// crate (`:crash_key_rust_for_testing`), which enable the `testing`
    /// feature.
    #[cfg(feature = "testing")]
    pub fn as_annotation_ptr_for_testing(&self) -> *const c_void {
        std::ptr::from_ref(&self.header).cast()
    }
}

// SAFETY: `value` is only mutated while holding `writer_lock`. All other fields
// that may be mutated through a shared reference are atomics, and `name` is a
// pointer to immutable `'static` data. (Crashpad's crash handler reads this
// memory from another process while this process is suspended, and Crashpad's
// own in-process readers of annotation values are debug-only and already
// accept racing with writers, as for C++ crash keys.)
unsafe impl<const N: usize> Sync for CrashKeyString<N> {}

/// Releases `CrashKeyString::writer_lock` when dropped.
struct WriterLockGuard<'a> {
    lock: &'a AtomicBool,
}

impl Drop for WriterLockGuard<'_> {
    fn drop(&mut self) {
        self.lock.store(false, Ordering::Release);
    }
}

/// Formats into a fixed-size buffer, truncating at a character boundary once
/// the buffer is full.
struct TruncatingWriter<'a> {
    buffer: &'a mut [u8],
    /// The number of bytes of `buffer` that have been written.
    len: usize,
}

impl fmt::Write for TruncatingWriter<'_> {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        let available = self.buffer.len() - self.len;
        let truncated = truncate_to_char_boundary(s, available);
        self.buffer[self.len..self.len + truncated.len()].copy_from_slice(truncated.as_bytes());
        self.len += truncated.len();
        // Stop formatting once the buffer is full.
        if truncated.len() < s.len() {
            Err(fmt::Error)
        } else {
            Ok(())
        }
    }
}

/// Returns the longest prefix of `value` that is at most `max_len` bytes long
/// and ends on a character boundary.
fn truncate_to_char_boundary(value: &str, max_len: usize) -> &str {
    if value.len() <= max_len {
        return value;
    }
    let mut len = max_len;
    while !value.is_char_boundary(len) {
        len -= 1;
    }
    &value[..len]
}
