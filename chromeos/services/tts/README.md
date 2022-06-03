TtsService and TtsClient provide a private api to communicate with the
text-to-speech engine in Chrome OS.

The TtsClient remote lives in an extension which can be found in the Chrome OS
repository at:
third_party/chromiumos-overlay/app-accessibility/googletts/

The TtsService runs in a sandboxed process running the native engine (in the
form of a shared object).
