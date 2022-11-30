The origin trial component implements browser-side support for origin trials.

This component is meant to supplement the implementation that exists in Blink,
by supplying an implementation for persistent origin trials.

> NOTE: The implementation is currently limited to supporting persistent
> origin trials, and users who need to check non-persistent origin trials in
> the browser should continue to use one of the methods listed in
> http://crbug.com/1227440#c0.

The code is implemented as a component since it needs to be shared between
content embedders. It is exposed to the browser process through the 
`content::OriginTrialsControllerDelegate` interface, which can be accessed from
subclasses of `content::BrowserContext`. This interface provides access to
methods to determine if a given persistent origin trial is currently enabled
for a given domain.

Since persistent origin trials are considered to be enabled until the next page
load for a given domain, the enablement status is not linked to any individual
browser tab, unlike regular origin trials.

> TODO(crbug.com/1227440): Make non-persistent origin trials available through
> this component.