The URL Formatter component contains utilities to convert between URLs and
human-entered/human-readable strings.  Broadly, consuming human-entered URLs
happens via "fixup", which tries to make "reasonable" adjustments to strings to
convert them into URLs (e.g. auto-prepending schemes, but also many more, some
of which may be surprising).  Producing human-readable URLs happens via
"formatting", which can strip unimportant parts of the URL, unescape/decode
sections, etc.

These functions are meant to work in conjunction with the stricter, more limited
capabilities of GURL, and were originally designed for use with the omnibox,
though they've since been used in other parts of the UI as well.

Because these functions are powerful, it's possible to introduce security risks
with incautious use.  Be sure you understand what you need and what they're
doing before using them; don't just copy existing callers.
