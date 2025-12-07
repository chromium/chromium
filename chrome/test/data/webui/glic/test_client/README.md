# Glic test client

This is a test page that facilitates testing the API between the host and the
web client. It can be used for manual and automated testing.


## Instructions

1. Build and run it.

```bash
   chrome/test/data/webui/glic/test_client/serve.py -C out/Default
```

2. Run Chrome with `--glic-guest-url=http://localhost:8000/glic/test_client/index.html`

3. For first run also use `--glic-fre-url=http://localhost:8000/glic/test_client/fre.html`
