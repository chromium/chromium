# Fingerprinting Protection Filter

The fingerprinting_protection filter component deals with requests that may be fingerprinting.
In order to have comprehensive filtering of all resource loads we will need filtering logic
both within the browser and on renderer processes. The renderer-side filtering will also depend
on page-level activation in and communication from the browser.

It reuses some code from components/subresource_filter to filter resources using a new ruleset
and different activation conditions.

