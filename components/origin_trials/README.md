The origin trial component implements browser-side support for origin trials.

[toc]

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

## Critical Origin Trials

A server can set the `Critical-Origin-Trial: <trial name>` HTTP header on
responses as a way to make the browser restart the request immediately if the
indicated trial was not already enabled on the first request.
In order to take advantage of this, the server must also provide a valid trial
token for the origin trial in the `Origin-Trial: <valid trial token>` header,
so the browser can enable the trial before restart.

The `Critical-Origin-Trial` header can be set multiple times.

If the headers do not include a valid trial token, a request for restart will be ignored.

## Outstanding work

-  TODO(crbug.com/40189223): Make non-persistent origin trials available through this component.
-  TODO(crbug.com/40257045): Make subdomain matching work for third-party tokens.
