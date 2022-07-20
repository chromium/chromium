The origin trial component implements browser-side support for origin trials.

This component is meant to supplement the implementation that exists in Blink,
by supplying an implementation for persistent origin trials.

The code is implemented as a component since it needs to be shared between
content embedders, to make it easier to use origin trials in the browser
process.